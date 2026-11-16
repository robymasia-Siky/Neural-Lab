import os
import json
import subprocess
import torch
from torch.utils.data import Dataset
from transformers import (
    AutoTokenizer,
    AutoModelForCausalLM,
    BitsAndBytesConfig,
    TrainingArguments,
    Trainer,
    DataCollatorForSeq2Seq
)
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
                enc["labels"] = enc["input_ids"].copy()
                self.samples.append(enc)

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return self.samples[idx]

def run_qlora_training():
    gpu_name = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "CPU"
    print(f"[QLoRA] CUDA Available: {torch.cuda.is_available()} | Device: {gpu_name}", flush=True)

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

    train_dataset = StudioJsonDataset(DATASET_PATH, tokenizer, max_length=384)
    print(f"[QLoRA] Loaded {len(train_dataset)} tokenized studio examples.", flush=True)

    training_args = TrainingArguments(
        output_dir="training/checkpoints",
        per_device_train_batch_size=8,
        gradient_accumulation_steps=2,
        learning_rate=2e-4,
        num_train_epochs=2,
        fp16=True,
        logging_steps=25,
        save_strategy="no",
        optim="paged_adamw_8bit",
        report_to="none"
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=train_dataset,
        data_collator=DataCollatorForSeq2Seq(tokenizer=tokenizer, padding=True)
    )

    print("[QLoRA] Starting 4-bit fine-tuning on GPU...", flush=True)
    trainer.train()

    model.save_pretrained(ADAPTER_DIR)
    tokenizer.save_pretrained(ADAPTER_DIR)
    print(f"[QLoRA] LoRA adapter saved to {ADAPTER_DIR}", flush=True)

    print("[EXPORT] Merging LoRA weights into standalone 0.5B model...", flush=True)
    del model
    torch.cuda.empty_cache()

    base_fp16 = AutoModelForCausalLM.from_pretrained(BASE_MODEL, torch_dtype=torch.float16, device_map="cpu")
    merged = PeftModel.from_pretrained(base_fp16, ADAPTER_DIR)
    merged = merged.merge_and_unload()
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