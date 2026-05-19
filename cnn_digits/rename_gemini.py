import os
import glob
import re
import shutil
import time
from collections import defaultdict
from PIL import Image
from google import genai
from google.genai import types
from pydantic import BaseModel, Field

class TrafficLightTimer(BaseModel):
    reasoning: str = Field(description="Step-by-step thinking process. Compare contrast/brightness between positions. State which are brightly lit vs dim. Ensure the final number does not include faint ghost digits.")
    number: str = Field(description="The final glowing number as a string (max 3 digits, e.g., '45', '133'). ONLY consider actively lit digits with high contrast.")

def get_api_key():
    api_key = None
    if os.path.exists('.env'):
        with open('.env', 'r') as f:
            for line in f:
                if 'GENAI_API_KEY=' in line:
                    api_key = line.split('=')[1].strip().strip('"\'')
                    break
    return api_key

api_key = get_api_key()
if not api_key:
    print("API key NOT FOUND in .env")
    exit(1)

client = genai.Client(api_key=api_key)

def extract_digit(path, max_retries=3):
    try:
        img = Image.open(path)
        
        prompt = """
        This is an image of a countdown timer on a traffic light. 
        CRITICAL REQUIREMENT: The extracted digits MUST be derived ONLY from the LED segments that are ACTIVELY EMITTING BRIGHT LIGHT. MAXIMUM of 3 digits.
        IMPORTANT RULE: The traffic light number is typically UNDER 500. If your extracted number is > 500, you are almost certainly hallucinating a faint 'ghost' digit as a real lit digit. Re-evaluate your contrast comparison!

        Look at the timer from left to right:
        1. Identify and IGNORE any faint, dim, or unlit background shapes (these often look like dark '8's).
        2. COMPARE BRIGHTNESS/CONTRAST: All active digits MUST have the exact same high intensity/contrast. If one digit or segment is brightly lit while another is noticeably dimmer, the dim one is DEFINITELY NOT part of the real number. Completely ignore dim digits!
        3. Trace ONLY the fiercely glowing segments to determine the real digits.
        
        DO NOT include numbers from unlit or faint segments. If the leftmost display area is noticeably dimmer, do not extract any number from it (e.g., if you see '[dim] [bright 4] [bright 5]', the final number is just '45').
        """

        config = types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=TrafficLightTimer,
            temperature=0.1,  # Low temperature for more deterministic output
        )
        
        for attempt in range(max_retries):
            response = client.models.generate_content(
                model="gemini-flash-lite-latest",
                contents=[img, prompt],
                config=config
            )
            
            # Parsed object holds the structured output
            if response.parsed and response.parsed.number:
                ans = re.sub(r'\D', '', str(response.parsed.number))
                if ans:
                    num_val = int(ans)
                    if num_val > 500 and attempt < max_retries - 1:
                        print(f"[{path}] Model returned {num_val} (> 500). Retrying... (Attempt {attempt + 1}/{max_retries})")
                        time.sleep(1) # delay before retry
                        continue
                    return ans
                    
        print(f"[{path}] Model exhausted retries or returned unknown: {response.text}")
        return "unknown"
    except Exception as e:
        print(f"Error processing {path}: {e}")
        return "error"

def main():
    folder = "realDatasetRoi"
    files = glob.glob(f"{folder}/*.*")
    # Exclude the csv file
    files = [f for f in files if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp'))]
    files.sort()
    
    if not files:
        print("No images found to process.")
        return

    print(f"Found {len(files)} images to rename with Gemini.")
    
    # Store old names
    counters = defaultdict(int)
    mapping = {}
    
    # Process sequentially to avoid rate limits
    for i, path in enumerate(files, 1):
        filename = os.path.basename(path)
        
        digit = extract_digit(path)
        
        # Increment finding counter
        counters[digit] += 1
        idx = counters[digit]
        
        ext = os.path.splitext(filename)[1]
        new_name = f"{digit}_{idx:04d}{ext}"
        new_path = os.path.join(folder, new_name)
        
        # Ensure we don't accidentally overwrite an existing file
        while os.path.exists(new_path) and new_path != path:
            idx += 1
            counters[digit] = idx
            new_name = f"{digit}_{idx:04d}{ext}"
            new_path = os.path.join(folder, new_name)
            
        mapping[path] = new_path
        print(f"[{i}/{len(files)}] {filename} -> {new_name}")
        
        if path != new_path:
            os.rename(path, new_path)
            
        # tiny sleep just to be gentle on ratelimits
        time.sleep(0.3)

    print("All done!")

if __name__ == "__main__":
    main()
