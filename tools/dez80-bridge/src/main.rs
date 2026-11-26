//! Thin CLI for the DeZ80 crate. No emulator instruction decoding lives here.
use dez80::Instruction;
use std::env;

fn decode_hex(input: &str) -> Result<Vec<u8>, String> {
    if input.len() % 2 != 0 {return Err("odd-length hex input".into());}
    input.as_bytes().chunks_exact(2).map(|pair| {
        let digits = std::str::from_utf8(pair).map_err(|e| e.to_string())?;
        u8::from_str_radix(digits, 16).map_err(|e| e.to_string())
    }).collect()
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {return Err("Usage: fm11-dez80-bridge COUNT HEX_BYTES".into());}
    let count: usize = args[1].parse()?;
    if !(1..=64).contains(&count) {return Err("count must be 1..64".into());}
    let bytes = decode_hex(&args[2])?;
    let mut remaining: &[u8] = &bytes;
    for _ in 0..count {
        if remaining.is_empty() {break;}
        let previous = remaining.len();
        match Instruction::decode_one(&mut remaining) {
            Ok(instruction) => {
                let used = previous - remaining.len();
                if used == 0 {return Err("DeZ80 returned a zero-length instruction".into());}
                println!("{}\t{}", used, instruction);
            }
            Err(_) => {
                // DeZ80 could not complete an instruction in the provided bytes.
                // Do not invent an instruction length or silently skip the bytes.
                break;
            }
        }
    }
    Ok(())
}
