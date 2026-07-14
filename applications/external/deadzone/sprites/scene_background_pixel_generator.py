import cv2
import numpy as np

# Load the image
image = cv2.imread("scene_background.png")

# If the image is loaded successfully
if image is not None:
    # Convert the image to grayscale (if it's a color image)
    gray_image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

    # Define the threshold to detect "dark" pixels
    # You can adjust the threshold as needed (e.g., 50 for dark pixels, 100 for less dark)
    dark_threshold = 50

    # Create a mask that identifies pixels with intensity below the threshold (dark pixels)
    dark_pixels_mask = gray_image < dark_threshold

    # Get the coordinates of all dark pixels
    dark_pixel_coordinates = np.column_stack(np.where(dark_pixels_mask))

    # Print and save the coordinates to a file
    with open("black_pixels.txt", "w") as file:
        for coord in dark_pixel_coordinates:
            file.write(f"{{{coord[1]}, {coord[0]}}},\n")  # Format as {x, y}

    print(
        f"Found {len(dark_pixel_coordinates)} black pixels. Coordinates saved to 'black_pixels.txt'."
    )
else:
    print("Image not found!")
