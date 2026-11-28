import ctypes
import json
import time
from pathlib import Path

import cv2
import numpy as np
import pyautogui


WINDOW_NAME = "IR Wii Tracker"
CALIBRATION_FILE = Path(__file__).with_name("calibration.json")
CALIBRATION_STEPS = (
    ("Center", (0.50, 0.50)),
    ("Top-Left", (0.16, 0.28)),
    ("Top-Right", (0.84, 0.28)),
    ("Bottom-Left", (0.16, 0.78)),
    ("Bottom-Right", (0.84, 0.78)),
)
DEFAULT_THRESHOLD = 150
DEFAULT_MAX_ROOM_LIGHT = 45
MIN_MAX_ROOM_LIGHT = 10
MAX_MAX_ROOM_LIGHT = 120
LED_BRIGHTNESS_EXCLUSION_RADIUS = 24
PURPLE_SCORE_THRESHOLD = 60
SMOOTHING_ALPHA = 0.2
MOVEMENT_DEADZONE = 4
LIGHTING_CHECK_SECONDS = 300
LIGHTING_CHANGE_THRESHOLD = 35
MORPHOLOGY_KERNEL = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (25, 25))

pyautogui.FAILSAFE = False
pyautogui.PAUSE = 0


def set_display_required(enabled):
    flags = 0x80000002 if enabled else 0x80000000
    if not ctypes.windll.kernel32.SetThreadExecutionState(flags):
        print("Warning: Windows could not update the display sleep state.")


def move_cursor(x, y, movement_paused):
    try:
        pyautogui.moveTo(int(round(x)), int(round(y)))
    except Exception as error:
        if not movement_paused:
            print(f"Cursor movement paused: {error}")
        return True
    if movement_paused:
        print("Cursor movement resumed.")
    return False


def load_calibration():
    try:
        with CALIBRATION_FILE.open("r", encoding="utf-8") as file:
            data = json.load(file)
        points = np.asarray(data["points"], dtype=np.float64)
        baseline = float(data["baseline_brightness"])
        threshold = int(data.get("threshold", DEFAULT_THRESHOLD))
        if (
            points.shape != (5, 2)
            or not np.isfinite(points).all()
            or np.any(points < 0)
            or np.any(points > 1)
            or not np.isfinite(baseline)
            or not 0 <= baseline <= 255
            or not 0 <= threshold <= 255
        ):
            print("Calibration data is invalid; starting calibration.")
            return None
        return {
            "points": points.tolist(),
            "baseline_brightness": baseline,
            "threshold": threshold,
        }
    except FileNotFoundError:
        return None
    except (OSError, ValueError, TypeError, KeyError, OverflowError) as error:
        print(f"Could not load calibration: {error}")
        return None


def make_calibration(points, width, height, baseline, threshold):
    normalized = [[x / max(width - 1, 1), y / max(height - 1, 1)] for x, y in points]
    return {
        "points": normalized,
        "baseline_brightness": float(baseline),
        "threshold": int(threshold),
    }


def write_calibration(calibration):
    with CALIBRATION_FILE.open("w", encoding="utf-8") as file:
        json.dump(calibration, file, indent=2)
        file.write("\n")


def build_perspective_transform(
    calibration, width, height, screen_width, screen_height
):
    source = np.asarray(calibration["points"][1:], dtype=np.float32)
    source *= np.array([max(width - 1, 1), max(height - 1, 1)], dtype=np.float32)
    hull = cv2.convexHull(source.reshape(-1, 1, 2))
    if cv2.contourArea(hull) < 4:
        return None
    destination = np.array(
        [
            [0, 0],
            [screen_width - 1, 0],
            [0, screen_height - 1],
            [screen_width - 1, screen_height - 1],
        ],
        dtype=np.float32,
    )
    transform = cv2.getPerspectiveTransform(source, destination)
    return transform if np.isfinite(transform).all() else None


def find_led(gray, frame, threshold, region=None):
    _, brightness = cv2.threshold(gray, int(threshold), 255, cv2.THRESH_BINARY)
    blue, green, red = cv2.split(frame.astype(np.float32))
    purple = (
        (red + blue - 1.5 * green > PURPLE_SCORE_THRESHOLD)
        & (red > green)
        & (blue > green)
    ).astype(np.uint8) * 255
    mask = cv2.bitwise_and(brightness, purple)
    offset_x = offset_y = 0
    if region is not None:
        x0, y0, x1, y1 = region
        mask, offset_x, offset_y = mask[y0:y1, x0:x1], x0, y0
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, MORPHOLOGY_KERNEL)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None
    moments = cv2.moments(max(contours, key=cv2.contourArea))
    if moments["m00"] == 0:
        return None
    x = int(moments["m10"] / moments["m00"]) + offset_x
    y = int(moments["m01"] / moments["m00"]) + offset_y
    return x, y


def average_room_brightness(gray, blob):
    if blob is None:
        return float(gray.mean())
    room_mask = np.full(gray.shape, 255, dtype=np.uint8)
    cv2.circle(
        room_mask,
        (int(blob[0]), int(blob[1])),
        LED_BRIGHTNESS_EXCLUSION_RADIUS,
        0,
        -1,
    )
    room_pixels = gray[room_mask != 0]
    return float(room_pixels.mean()) if room_pixels.size else float(gray.mean())


def draw_room_light_warning(frame, brightness, limit):
    height, width = frame.shape[:2]
    left, right = 8, max(9, width - 8)
    banner_height = min(112, max(72, height - 12))
    top = max(0, (height - banner_height) // 2)
    bottom = min(height - 1, top + banner_height)
    cv2.rectangle(frame, (left, top), (right, bottom), (0, 0, 190), -1)
    cv2.rectangle(frame, (left, top), (right, bottom), (0, 0, 255), 3)

    title = "LIGHTS MUST BE OFF TO TRACK"
    font = cv2.FONT_HERSHEY_DUPLEX
    title_scale = 0.9
    title_width = cv2.getTextSize(title, font, title_scale, 2)[0][0]
    title_scale *= min(1.0, (right - left - 20) / max(title_width, 1))
    title_size = cv2.getTextSize(title, font, title_scale, 2)[0]
    title_x = max(left + 8, (width - title_size[0]) // 2)
    title_y = top + banner_height // 2 - 2
    cv2.putText(frame, title, (title_x, title_y), font, title_scale, (255, 255, 255), 2)

    detail = f"Room Brightness: {brightness:.0f} / {limit}"
    detail_scale = min(0.65, title_scale * 0.75)
    detail_width = cv2.getTextSize(detail, font, detail_scale, 1)[0][0]
    detail_x = max(left + 8, (width - detail_width) // 2)
    cv2.putText(
        frame,
        detail,
        (detail_x, min(bottom - 8, title_y + 28)),
        font,
        detail_scale,
        (255, 255, 255),
        1,
    )


def draw_calibration(frame, step, blob, threshold):
    height, width = frame.shape[:2]
    half_size = max(24, int(min(width, height) * 0.065))
    for index, (name, fraction) in enumerate(CALIBRATION_STEPS):
        x, y = int(fraction[0] * (width - 1)), int(fraction[1] * (height - 1))
        color = (80, 255, 110) if index == step else (130, 150, 170)
        cv2.rectangle(
            frame,
            (x - half_size, y - half_size),
            (x + half_size, y + half_size),
            color,
            2,
        )
        cv2.putText(
            frame,
            f"{index + 1}. {name}",
            (x - half_size, y - half_size - 5),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.45,
            color,
            1,
        )

    fx, fy = CALIBRATION_STEPS[step][1]
    x, y = int(fx * (width - 1)), int(fy * (height - 1))
    body = np.array(
        [
            [x - 13, y - 27],
            [x + 8, y - 27],
            [x + 15, y - 20],
            [x + 15, y + 24],
            [x - 13, y + 24],
            [x - 19, y + 18],
            [x - 19, y - 20],
        ],
        dtype=np.int32,
    )
    cv2.polylines(frame, [body], True, (235, 235, 245), 2)
    cv2.rectangle(frame, (x + 4, y - 11), (x + 12, y + 11), (55, 20, 65), -1)
    for dx, dy in ((8, -6), (6, 4), (11, 6)):
        cv2.circle(frame, (x + dx, y + dy), 2, (220, 65, 255), -1)
    cv2.putText(
        frame,
        f"Step {step + 1}/5: Aim in the box; SPACE saves",
        (12, 24),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        2,
    )
    cv2.putText(
        frame,
        f"Threshold {threshold} | C restart | Q quit",
        (12, 48),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.45,
        (220, 220, 220),
        1,
    )
    if blob is not None:
        cv2.circle(frame, blob, 8, (0, 255, 0), 2)
    return frame


def main():
    calibration = load_calibration()
    calibrating = calibration is None
    points = []
    baseline = calibration["baseline_brightness"] if calibration else None
    threshold = calibration["threshold"] if calibration else DEFAULT_THRESHOLD
    transform = None
    smoothed = None
    lighting_warning = False
    last_lighting_check = None
    room_was_too_bright = False
    movement_paused = False
    camera = None

    try:
        set_display_required(True)
        screen_width, screen_height = pyautogui.size()
        camera = cv2.VideoCapture(0)
        if not camera.isOpened():
            print("Error: webcam 0 could not be opened.")
            return
        for _ in range(10):
            camera.read()

        cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.createTrackbar(
            "Threshold", WINDOW_NAME, int(threshold), 255, lambda _: None
        )
        cv2.createTrackbar(
            "Max Room Light",
            WINDOW_NAME,
            int(DEFAULT_MAX_ROOM_LIGHT - MIN_MAX_ROOM_LIGHT),
            MAX_MAX_ROOM_LIGHT - MIN_MAX_ROOM_LIGHT,
            lambda _: None,
        )
        cv2.resizeWindow(WINDOW_NAME, 800, 600)

        while True:
            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), ord("Q"), 27):
                break
            if key in (ord("c"), ord("C")):
                calibration, calibrating, points, baseline = None, True, [], None
                threshold, transform, smoothed = DEFAULT_THRESHOLD, None, None
                lighting_warning, last_lighting_check = False, None
                room_was_too_bright = False
                cv2.setTrackbarPos("Threshold", WINDOW_NAME, int(threshold))
                CALIBRATION_FILE.unlink(missing_ok=True)

            ret, frame = camera.read()
            if not ret or frame is None or frame.size == 0:
                continue

            frame = cv2.flip(frame, 1)
            height, width = frame.shape[:2]
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            threshold = int(cv2.getTrackbarPos("Threshold", WINDOW_NAME))
            max_room_light = min(
                MAX_MAX_ROOM_LIGHT,
                max(
                    MIN_MAX_ROOM_LIGHT,
                    int(cv2.getTrackbarPos("Max Room Light", WINDOW_NAME))
                    + MIN_MAX_ROOM_LIGHT,
                ),
            )

            if calibration is not None and transform is None:
                transform = build_perspective_transform(
                    calibration, width, height, screen_width, screen_height
                )
                if transform is None:
                    print("Saved calibration is invalid; press C to recalibrate.")
                    calibration, calibrating, points, baseline = None, True, [], None
                    threshold, smoothed = DEFAULT_THRESHOLD, None
                    cv2.setTrackbarPos("Threshold", WINDOW_NAME, int(threshold))

            if calibrating:
                step = len(points)
                fraction = CALIBRATION_STEPS[step][1]
                x, y = int(fraction[0] * (width - 1)), int(fraction[1] * (height - 1))
                half = max(24, int(min(width, height) * 0.065))
                region = (
                    max(0, x - half),
                    max(0, y - half),
                    min(width, x + half + 1),
                    min(height, y + half + 1),
                )
                blob = find_led(gray, frame, threshold, region)
            else:
                blob = find_led(gray, frame, threshold)

            room_brightness = average_room_brightness(gray, blob)
            room_too_bright = room_brightness > max_room_light
            if room_was_too_bright and not room_too_bright:
                lighting_warning = False
            room_was_too_bright = room_too_bright

            if calibration is not None and not calibrating:
                now = time.time()
                if (
                    last_lighting_check is None
                    or now - last_lighting_check >= LIGHTING_CHECK_SECONDS
                ):
                    last_lighting_check = now
                    changed = (
                        abs(room_brightness - baseline) > LIGHTING_CHANGE_THRESHOLD
                    )
                    lighting_warning = lighting_warning or changed

            if calibrating:
                preview = draw_calibration(frame.copy(), step, blob, threshold)
            else:
                if (
                    blob is not None
                    and transform is not None
                    and not lighting_warning
                    and not room_too_bright
                ):
                    mapped = cv2.perspectiveTransform(
                        np.array([[blob]], dtype=np.float32), transform
                    )[0, 0]
                    target = (
                        float(np.clip(mapped[0], 0, screen_width - 1)),
                        float(np.clip(mapped[1], 0, screen_height - 1)),
                    )
                    if smoothed is None:
                        smoothed = target
                        movement_paused = move_cursor(*target, movement_paused)
                    elif (
                        np.hypot(target[0] - smoothed[0], target[1] - smoothed[1])
                        >= MOVEMENT_DEADZONE
                    ):
                        smoothed = (
                            SMOOTHING_ALPHA * target[0]
                            + (1 - SMOOTHING_ALPHA) * smoothed[0],
                            SMOOTHING_ALPHA * target[1]
                            + (1 - SMOOTHING_ALPHA) * smoothed[1],
                        )
                        movement_paused = move_cursor(*smoothed, movement_paused)
                preview = frame.copy()
                if blob is not None:
                    cv2.circle(preview, blob, 9, (0, 255, 0), 2)
                cv2.putText(
                    preview,
                    f"Threshold {threshold} | C recalibrate | Q quit",
                    (12, 24),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (255, 255, 255),
                    1,
                )
            cv2.putText(
                preview,
                f"Room Brightness: {room_brightness:.0f} / {max_room_light}",
                (12, 72),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (255, 255, 255),
                1,
            )

            if room_too_bright:
                draw_room_light_warning(preview, room_brightness, max_room_light)
            elif lighting_warning:
                cv2.putText(
                    preview,
                    "Lighting changed! Press C to recalibrate",
                    (12, 58),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6,
                    (0, 0, 255),
                    2,
                )
            cv2.imshow(WINDOW_NAME, preview)

            if calibrating and key == ord(" "):
                if room_too_bright:
                    continue
                if blob is None:
                    print(f"Step {len(points) + 1}: no LED detected in the target box.")
                    continue
                candidate_points = points + [blob]
                candidate_baseline = room_brightness if baseline is None else baseline
                candidate = make_calibration(
                    candidate_points, width, height, candidate_baseline, threshold
                )
                candidate_transform = None
                if len(candidate_points) == len(CALIBRATION_STEPS):
                    candidate_transform = build_perspective_transform(
                        candidate, width, height, screen_width, screen_height
                    )
                    if candidate_transform is None:
                        print(
                            "Calibration corners too close; capture the last point again."
                        )
                        continue
                write_calibration(candidate)
                points, baseline = candidate_points, candidate_baseline
                if candidate_transform is not None:
                    calibration, transform, calibrating = (
                        candidate,
                        candidate_transform,
                        False,
                    )
                    last_lighting_check = time.time()
                    print(f"Calibration saved to {CALIBRATION_FILE}.")
    finally:
        set_display_required(False)
        if camera is not None:
            camera.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
