#!/usr/bin/env python3
"""
Period Tracker Test Data Generator

Generates comprehensive test data for the Period Tracker application including:
- Multiple user profiles with different characteristics
- Historical cycle data (CSV files)
- Profile index file
- Realistic symptoms and patterns

The generator creates data positioned to show alerts for today, making it
ideal for testing the Daily Digest and prediction features.

Usage:
    python3 generate_test_data.py

Output files:
    - *.profile - Profile configuration files
    - *.csv - Event history files
    - profiles.idx - Profile index
"""

import os
import csv
from datetime import datetime, timedelta
from pathlib import Path
import random

# Profile templates with realistic configurations
PROFILES = [
    {
        'name': 'Alice',
        'cycle_length_days': 28,
        'period_length_days': 5,
        'uses_contraception': False,
        'has_pcos': False,
        'is_menopausal': False,
        'notes': 'Regular cycles, generally healthy',
        'num_cycles': 6,  # Number of historical cycles to generate
        'symptom_intensity': 'moderate',  # light, moderate, heavy
    },
    {
        'name': 'Emma',
        'cycle_length_days': 35,
        'period_length_days': 6,
        'uses_contraception': False,
        'has_pcos': True,
        'is_menopausal': False,
        'notes': 'PCOS - irregular cycles',
        'num_cycles': 5,
        'symptom_intensity': 'heavy',
    },
    {
        'name': 'Sarah',
        'cycle_length_days': 28,
        'period_length_days': 4,
        'uses_contraception': True,
        'has_pcos': False,
        'is_menopausal': False,
        'notes': 'On birth control, very regular',
        'num_cycles': 6,
        'symptom_intensity': 'light',
    },
    {
        'name': 'Olivia',
        'cycle_length_days': 32,
        'period_length_days': 4,
        'uses_contraception': False,
        'has_pcos': False,
        'is_menopausal': True,  # Perimenopause
        'notes': 'Perimenopause - increasingly irregular cycles',
        'num_cycles': 7,
        'symptom_intensity': 'moderate',
        'cycle_irregularity': True,  # Flag for irregular cycle generation
    },
]

# Available symptoms by intensity
SYMPTOMS = {
    'light': ['Cramps', 'Fatigue', 'Bloating'],
    'moderate': ['Cramps', 'Headache', 'Mood Swings', 'Bloating', 'Fatigue', 'Back Pain'],
    'heavy': ['Cramps', 'Headache', 'Mood Swings', 'Bloating', 'Fatigue', 'Nausea', 'Back Pain', 'Breast Tenderness'],
}

def generate_profile_file(profile, output_dir):
    """Generate a .profile file"""
    filename = output_dir / f"{profile['name']}.profile"

    with open(filename, 'w') as f:
        f.write(f"name={profile['name']}\n")
        f.write(f"cycle_length_days={profile['cycle_length_days']}\n")
        f.write(f"period_length_days={profile['period_length_days']}\n")
        f.write(f"uses_contraception={1 if profile['uses_contraception'] else 0}\n")
        f.write(f"has_pcos={1 if profile['has_pcos'] else 0}\n")
        f.write(f"is_menopausal={1 if profile['is_menopausal'] else 0}\n")
        f.write(f"notes={profile['notes']}\n")

    print(f"  Created: {filename.name}")

def generate_cycle_events(profile, cycle_start_date):
    """Generate events for one menstrual cycle"""
    events = []

    # Period start event
    events.append({
        'date': cycle_start_date,
        'event_type': 'period_start',
        'value': ''
    })

    # Get symptoms for this profile's intensity
    available_symptoms = SYMPTOMS[profile['symptom_intensity']]

    # Day 1: Period start symptoms (always cramps)
    events.append({
        'date': cycle_start_date,
        'event_type': 'symptom',
        'value': 'Cramps'
    })

    # Days 2-3: Additional period symptoms
    period_days = min(3, profile['period_length_days'])
    for day_offset in range(1, period_days):
        date = cycle_start_date + timedelta(days=day_offset)
        # Add 1-2 random symptoms per day
        num_symptoms = random.randint(1, 2)
        symptoms_for_day = random.sample([s for s in available_symptoms if s != 'Cramps'],
                                        min(num_symptoms, len(available_symptoms) - 1))
        for symptom in symptoms_for_day:
            events.append({
                'date': date,
                'event_type': 'symptom',
                'value': symptom
            })

    # PMS symptoms (3-5 days before next period, which is the end of this cycle)
    if not profile['uses_contraception']:  # Less PMS on birth control
        pms_start = cycle_start_date + timedelta(days=profile['cycle_length_days'] - random.randint(3, 5))
        if pms_start > cycle_start_date + timedelta(days=period_days):
            # Add 1-2 PMS symptoms
            pms_symptoms = random.sample([s for s in available_symptoms if s in ['Mood Swings', 'Bloating', 'Headache', 'Fatigue']],
                                        min(2, len(available_symptoms)))
            for symptom in pms_symptoms:
                events.append({
                    'date': pms_start,
                    'event_type': 'symptom',
                    'value': symptom
                })

    return events

def generate_csv_file(profile, output_dir):
    """Generate a .csv file with historical cycle data"""
    filename = output_dir / f"{profile['name']}.csv"

    today = datetime.now()

    # Calculate when the most recent period should start
    # Position it at (cycle_length - 3) days ago to generate alerts for today/tomorrow
    days_ago = profile['cycle_length_days'] - 3
    most_recent_period = today - timedelta(days=days_ago)

    # Generate historical cycles working backwards
    all_events = []
    current_cycle_start = most_recent_period

    for cycle_num in range(profile['num_cycles']):
        # Generate events for this cycle
        cycle_events = generate_cycle_events(profile, current_cycle_start)
        all_events.extend(cycle_events)

        # Move to previous cycle
        # Add variation for irregular profiles
        if cycle_num < profile['num_cycles'] - 1:
            if profile['has_pcos']:
                # PCOS: irregular cycles (±5 days variation)
                cycle_length = profile['cycle_length_days'] + random.randint(-5, 5)
            elif profile.get('cycle_irregularity', False):
                # Perimenopause: increasingly irregular (±7 days, increasing variation over time)
                # More recent cycles are more irregular
                variation_range = min(7, 3 + cycle_num)  # Starts at ±3, grows to ±7
                cycle_length = profile['cycle_length_days'] + random.randint(-variation_range, variation_range)
            else:
                cycle_length = profile['cycle_length_days']
        else:
            cycle_length = profile['cycle_length_days']

        current_cycle_start = current_cycle_start - timedelta(days=cycle_length)

    # Sort events by date
    all_events.sort(key=lambda e: e['date'])

    # Write CSV
    with open(filename, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['date', 'event_type', 'value'])
        writer.writeheader()
        for event in all_events:
            writer.writerow({
                'date': event['date'].strftime('%Y-%m-%d'),
                'event_type': event['event_type'],
                'value': event['value']
            })

    print(f"  Created: {filename.name}")
    print(f"    Events: {len(all_events)}")
    print(f"    Date range: {all_events[0]['date'].strftime('%Y-%m-%d')} to {all_events[-1]['date'].strftime('%Y-%m-%d')}")
    print(f"    Last period: {most_recent_period.strftime('%Y-%m-%d')} ({days_ago} days ago)")

def generate_profiles_index(profiles, output_dir):
    """Generate profiles.idx file"""
    filename = output_dir / "profiles.idx"

    with open(filename, 'w') as f:
        for profile in profiles:
            f.write(f"{profile['name']}\n")

    print(f"  Created: {filename.name}")
    print(f"    Profiles: {len(profiles)}")

def generate_archived_profile(output_dir):
    """Generate an archived test profile with dates relative to today."""
    # Profile file
    profile_filename = output_dir / "TestArchived.archive.profile"
    with open(profile_filename, 'w') as f:
        f.write("name=TestArchived\n")
        f.write("cycle_length_days=28\n")
        f.write("period_length_days=5\n")
        f.write("uses_contraception=0\n")
        f.write("has_pcos=0\n")
        f.write("is_menopausal=0\n")
        f.write("notes=This is an archived profile for testing\n")

    # One old cycle so the archive still has valid history
    archived_start = datetime.now() - timedelta(days=120)
    date_str = archived_start.strftime('%Y-%m-%d')

    csv_filename = output_dir / "TestArchived.archive.csv"
    with open(csv_filename, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['date', 'event_type', 'value'])
        writer.writeheader()
        writer.writerow({
            'date': date_str,
            'event_type': 'period_start',
            'value': ''
        })
        writer.writerow({
            'date': date_str,
            'event_type': 'symptom',
            'value': 'Cramps'
        })

    print(f"  Created: {profile_filename.name}")
    print(f"  Created: {csv_filename.name}")
    print(f"    Archived period: {date_str}")

def main():
    """Main function to generate all test data"""
    script_dir = Path(__file__).parent

    print("=" * 60)
    print("Period Tracker Test Data Generator")
    print("=" * 60)
    print(f"Output directory: {script_dir}")
    print(f"Generation date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()

    # Set random seed for reproducibility (optional, comment out for randomness)
    random.seed(42)

    # Generate profiles
    print("Generating profiles:")
    print("-" * 60)
    for profile in PROFILES:
        print(f"\n{profile['name']}:")
        print(f"  Cycle: {profile['cycle_length_days']} days")
        print(f"  Period: {profile['period_length_days']} days")
        print(f"  PCOS: {profile['has_pcos']}")
        print(f"  Contraception: {profile['uses_contraception']}")

        generate_profile_file(profile, script_dir)
        generate_csv_file(profile, script_dir)

    # Generate profiles index
    print("\n" + "-" * 60)
    print("\nGenerating index:")
    generate_profiles_index(PROFILES, script_dir)

    # Generate archived profile
    print("\nGenerating archived profile:")
    generate_archived_profile(script_dir)

    print("\n" + "=" * 60)
    print("✓ Test data generation complete!")
    print("=" * 60)
    print()
    print("Files created:")
    print("  - 4 active profiles (.profile + .csv files)")
    print("  - 1 archived profile (.archive.profile + .archive.csv files)")
    print("  - 1 profiles index (profiles.idx)")
    print()
    print("Expected behavior:")
    print("  - Daily Digest should show multiple alerts for today/tomorrow")
    print("  - Each profile positioned to generate upcoming predictions")
    print("  - Realistic symptom patterns based on intensity levels")
    print()

if __name__ == "__main__":
    main()
