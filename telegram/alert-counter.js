// Shared daily-limit decision for the KV and Durable Object counters.
// A null next state means the counter needs no storage write.
export function hitCounter(current, limit) {
  if (current.n >= limit) {
    const noteNow = !current.noted;
    return {
      next: noteNow ? { ...current, noted: true } : null,
      result: { n: current.n, allowed: false, noteNow },
    };
  }
  const next = { ...current, n: current.n + 1 };
  return { next, result: { n: next.n, allowed: true, noteNow: false } };
}
