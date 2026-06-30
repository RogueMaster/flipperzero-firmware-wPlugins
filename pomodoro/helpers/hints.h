#pragma once

// Hints for focus stage
static char* work_hints[] = {
    "Can you explain the problem as if I'm five?",
    "Expected output vs. reality: what's the difference?",
    "Ever thought of slicing the problem into bite-sized pieces?",
    "What's the story when you walk through the code?",
    "Any error messages gossiping about the issue?",
    "What tricks have you tried to fix this?",
    "Did you test the code, or just hoping for the best?",
    "How's this code mingling with the rest of the app?",
    "Any sneaky side effects causing mischief?",
    "What are you assuming, and is it safe to do so?",
    "Did you remember to invite all the edge cases to the party?",
    "What happens in the isolation chamber (running code separately)?",
    "Can you make the issue appear on command?",
    "What's the scene at the crime spot when the error occurs?",
    "Did you seek wisdom from the grand oracle (Google)?",
    "What if you take a different path to solve this?",
    "Did you take a coffee break to reboot your brain?",
    "What part of the code do you trust the least?",
    "Did the bug appear after a recent change?",
    "What would happen if you removed this block entirely?",
    "Is the code doing too much in one place?",
    "If this were someone else's bug, how would you debug it?"
};

// Hints for break stages
static char* break_hints[] = {
    "Time to stretch! Remember, your body isn't made of code.",
    "Hydrate or diedrate! Grab a glass of water.",
    "Blink! Your eyes need a break too.",
    "How about a quick dance-off with your shadow?",
    "Ever tried chair yoga? Now's the time!",
    "Time for a quick peek out the window. The outside world still exists!",
    "Quick, think about kittens! Or puppies! Or baby turtles!",
    "Time for a laugh. Look up a joke or two!",
    "Sing a song. Bonus points for making up your own lyrics.",
    "Do a quick tidy-up. A clean space is a happy space!",
    "Time to play 'air' musical instrument for a minute.",
    "How about a quick doodle? Unleash your inner Picasso!",
    "Practice your superhero pose. Feel the power surge!",
    "Quick, tell yourself a joke. Don't worry, I won't judge.",
    "Time to practice your mime skills. Stuck in a box, anyone?",
    "Ever tried juggling? Now's your chance!",
    "Do a quick self high-five, you're doing great!",
    "Close your eyes for 30 seconds and just breathe.",
    "Step away from the screen — literally 10 steps back.",
    "Name 3 things you can hear right now. Ground yourself.",
    "Imagine finishing this task and how it’ll feel.",
    "Stand up and shake your hands like you're drying them."
};

#define WORK_HINTS_COUNT  (sizeof(work_hints) / sizeof(work_hints[0]))
#define BREAK_HINTS_COUNT (sizeof(break_hints) / sizeof(break_hints[0]))
