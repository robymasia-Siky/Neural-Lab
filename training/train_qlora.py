import os
import subprocess
import torch
from datasets import load_dataset
from transformers import AutoTokenizer, AutoModelForCausalLM, BitsAndBytesConfig, TrainingArguments
from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training, PeftModel
from trl import SFTTrainer

BASE_MODEL = "Qwen/Qwen2.5-0.5B-Instruct"
DATASET_PATH = "training/neural_lab_dataset.jsonl"
ADAPTER_DIR = "training/neural_lab_lora_adapter"
MERGED_DIR = "training/neural_lab_merged_0.5b"
OLLAMA_MODEL_NAME = "neural-lab-0.5b"

def run_qlora_training():
    print(f"[QLoRA] CUDA Available: {torch.cuda.is_available()} | Device: {torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU'}")

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

    dataset = load_dataset("json", data_files=DATASET_PATH, split="train")

    def format_chat(example):
        text = tokenizer.apply_chat_template(example["messages"], tokenize=False, add_generation_prompt=False)
        return {"text": text}

    dataset = dataset.map(format_chat)

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

    trainer = SFTTrainer(
        model=model,
        train_dataset=dataset,
        dataset_text_field="text",
        max_seq_length=384,
        tokenizer=tokenizer,
        args=training_args
    )

    print("[QLoRA] Starting 4-bit fine-tuning on 2,000 studio examples...")
    trainer.train()

    model.save_pretrained(ADAPTER_DIR)
    tokenizer.save_pretrained(ADAPTER_DIR)
    print(f"[QLoRA] LoRA adapter saved to {ADAPTER_DIR}")

    # Merge LoRA weights in FP16 per esportazione diretta in Ollama
    print("[EXPORT] Merging LoRA weights into standalone 0.5B model...")
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

    print(f"[OLLAMA] Registering fine-tuned model '{OLLAMA_MODEL_NAME}'...")
    subprocess.run(["ollama", "create", OLLAMA_MODEL_NAME, "-f", modelfile_path], check=False)
    print(f"[DONE] Fine-tuning complete! NEURAL LAB now uses '{OLLAMA_MODEL_NAME}'.")

if __name__ == "__main__":
    run_qlora_training()