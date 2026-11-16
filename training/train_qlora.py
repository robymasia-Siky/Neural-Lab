import os
import json
import subprocess
import torch
from torch.utils.data import Dataset, DataLoader
from transformers import AutoTokenizer, AutoModelForCausalLM, BitsAndBytesConfig, get_cosine_schedule_with_warmup
from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training, PeftModel

BASE_MODEL = "Qwen/Qwen2.5-0.5B-Instruct"
DATASET_PATH = "training/neural_lab_dataset.jsonl"
ADAPTER_DIR = "training/neural_lab_lora_adapter"
MERGED_DIR = "training/neural_lab_merged_0.5b"
OLLAMA_MODEL_NAME = "neural-lab-0.5b"

class StudioJsonDataset(Dataset):
    def __init__(self, jsonl_path: str, tokenizer, max_length: int = 384):
        self.samples = []
        with open(jsonl_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                item = json.loads(line)
                text = tokenizer.apply_chat_template(
                    item["messages"],
                    tokenize=False,
                    add_generation_prompt=False
                )
                enc = tokenizer(
                    text,
                    truncation=True,
                    max_length=max_length,
                    padding=False
                )
                self.samples.append(enc["input_ids"])

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return self.samples[idx]

def collate_batch(batch, pad_id: int):
    max_len = max(len(x) for x in batch)
    input_ids, attention_mask, labels = [], [], []
    for seq in batch:
        pad_len = max_len - len(seq)
        input_ids.append(seq + [pad_id] * pad_len)
        attention_mask.append([1] * len(seq) + [0] * pad_len)
        labels.append(seq + [-100] * pad_len)
    return {
        "input_ids": torch.tensor(input_ids, dtype=torch.long),
        "attention_mask": torch.tensor(attention_mask, dtype=torch.long),
        "labels": torch.tensor(labels, dtype=torch.long),
    }

def run_qlora_training():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    gpu_name = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "CPU"
    print(f"[QLoRA] Device: {gpu_name}", flush=True)

    bnb_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.float16,
        bnb_4bit_use_double_quant=True,
    )

    tokenizer = AutoTokenizer.from_pretrained(BASE_MODEL)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    model = AutoModelForCausalLM.from_pretrained(
        BASE_MODEL,
        quantization_config=bnb_config,
        device_map="auto"
    )
    model = prepare_model_for_kbit_training(model)

    lora_config = LoraConfig(
        r=16,
        lora_alpha=32,
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"],
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM"
    )
    model = get_peft_model(model, lora_config)
    model.print_trainable_parameters()

    dataset = StudioJsonDataset(DATASET_PATH, tokenizer, max_length=384)
    batch_size = 8
    grad_accum = 2
    epochs = 2
    loader = DataLoader(
        dataset,
        batch_size=batch_size,
        shuffle=True,
        collate_fn=lambda b: collate_batch(b, tokenizer.pad_token_id)
    )

    optimizer = torch.optim.AdamW(model.parameters(), lr=2e-4, weight_decay=0.01)
    total_steps = (len(loader) // grad_accum) * epochs
    scheduler = get_cosine_schedule_with_warmup(optimizer, num_warmup_steps=20, num_training_steps=total_steps)
    scaler = torch.amp.GradScaler("cuda", enabled=torch.cuda.is_available())

    print(f"[QLoRA] Training on {len(dataset)} examples ({epochs} epochs, {total_steps} optimizer steps)...", flush=True)
    model.train()
    global_step = 0
    running_loss = 0.0

    for epoch in range(1, epochs + 1):
        optimizer.zero_grad()
        for step, batch in enumerate(loader, 1):
            batch = {k: v.to(device) for k, v in batch.items()}
            with torch.amp.autocast("cuda", dtype=torch.float16, enabled=torch.cuda.is_available()):
                outputs = model(**batch)
                loss = outputs.loss / grad_accum

            scaler.scale(loss).backward()
            running_loss += loss.item() * grad_accum

            if step % grad_accum == 0 or step == len(loader):
                scaler.step(optimizer)
                scaler.update()
                optimizer.zero_grad()
                scheduler.step()
                global_step += 1

                if global_step % 25 == 0 or global_step == total_steps:
                    avg_loss = running_loss / 25.0 if global_step % 25 == 0 else running_loss / max(1, (global_step % 25))
                    print(f"Epoch {epoch}/{epochs} | Step {global_step}/{total_steps} | Loss: {avg_loss:.4f}", flush=True)
                    running_loss = 0.0

    os.makedirs(ADAPTER_DIR, exist_ok=True)
    model.save_pretrained(ADAPTER_DIR)
    tokenizer.save_pretrained(ADAPTER_DIR)
    print(f"[QLoRA] LoRA adapter saved to {ADAPTER_DIR}", flush=True)

    print("[EXPORT] Merging LoRA weights into standalone FP16 0.5B model...", flush=True)
    del model, optimizer, scaler
    torch.cuda.empty_cache()

    base_fp16 = AutoModelForCausalLM.from_pretrained(BASE_MODEL, torch_dtype=torch.float16, device_map="cpu")
    merged = PeftModel.from_pretrained(base_fp16, ADAPTER_DIR)
    merged = merged.merge_and_unload()
    os.makedirs(MERGED_DIR, exist_ok=True)
    merged.save_pretrained(MERGED_DIR, safe_serialization=True)
    tokenizer.save_pretrained(MERGED_DIR)

    modelfile_path = os.path.join(MERGED_DIR, "Modelfile")
    modelfile_content = (
        f"FROM {os.path.abspath(MERGED_DIR)}\n"
        "PARAMETER temperature 0.15\n"
        "PARAMETER num_ctx 1024\n"
        'SYSTEM "You are NEURAL LAB 0.5B Studio Orchestrator. Output ONLY strict JSON production plans."\n'
    )
    with open(modelfile_path, "w", encoding="utf-8") as mf:
        mf.write(modelfile_content)

    print(f"[OLLAMA] Registering fine-tuned model '{OLLAMA_MODEL_NAME}'...", flush=True)
    subprocess.run(["ollama", "create", OLLAMA_MODEL_NAME, "-f", modelfile_path], check=False)
    print(f"[DONE] Fine-tuning complete! NEURAL LAB now uses '{OLLAMA_MODEL_NAME}'.", flush=True)

if __name__ == "__main__":
    run_qlora_training()