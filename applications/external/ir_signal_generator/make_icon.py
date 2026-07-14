from PIL import Image
import os

# Path to the user uploaded image
# Note: The absolute path is provided in the prompt metadata
input_path = "/home/sacriphanius/.gemini/antigravity/brain/361c69ad-6e68-4f18-87b5-a7ce38841037/uploaded_media_1769802215347.png"

current_dir = os.path.dirname(os.path.abspath(__file__))
save_path = os.path.join(current_dir, "icon.png")

try:
    if not os.path.exists(input_path):
        print(f"Error: Input file not found at {input_path}")
        # Fallback to creating a dummy if file missing during test, but in this user flow it should exist
    else:
        # Open source image
        img = Image.open(input_path)

        # Resize to 10x10 using high quality downsampling
        img = img.resize((10, 10), Image.Resampling.LANCZOS)

        # Convert to 1-bit (Black and White).
        # Using a threshold might be better than simple conversion if it's grayscale
        # But '1' mode usually does dithering which is bad for 10x10.
        # Let's convert to grayscale 'L' first, then threshold.
        img = img.convert("L")

        # Simple thresholding
        threshold = 128
        img = img.point(lambda p: 255 if p > threshold else 0)

        # Convert to '1' mode
        img = img.convert("1")

        img.save(save_path)
        print(f"Icon processed and saved to {save_path}")

except Exception as e:
    print(f"Error processing image: {e}")
