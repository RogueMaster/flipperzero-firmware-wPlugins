# Optimal Blackjack

A blackjack **trainer** for the Flipper Zero. Nothing is wagered — your score is how
often your play matches **basic strategy**, so it teaches you to play a perfect game.

It also trains **card counting** and includes real advantage-play tools (house edge,
player edge, dollars per hour, N0 and risk of ruin).

## Modes

- **Play Table** — play hands against the dealer. Each decision is graded against basic
  strategy (S17 or H17); correct plays build a streak, a miss shows the right play.
- **Count Trainer** — cards flash by, you enter the running Hi-Lo count. 60 stages.
- **Endless Count** — the count trainer sped up until you miss.
- **Flashcards** — one hand at a time, pick the play, graded instantly.
- **Advantage** — a card counter's edge calculator: set the table rules, bankroll and
  bet spread and see your house edge, player edge, dollars per hour, N0 and risk of ruin.

## Tools

- **Strategy charts** — hard, soft and pair basic-strategy grids plus the Illustrious 18
  count deviations, for both S17 and H17.
- **Stats** — lifetime table and counting statistics.

## Controls

- **Left / Right / Up / Down** — Hit / Stand / Double / Split at the table (and in
  Flashcards); navigate the menus.
- **OK** — deal, confirm, or open the selected item.
- **Back** — return to the menu, or exit.

## Rules modelled

Six decks, dealer hits or stands on soft 17, Hi-Lo counting, hole card counted when it
flips. The Advantage calculator models deck count, soft-17, DAS, doubling and surrender
rules, blackjack payout, penetration, resplit/hit/double split aces and several promo
rules, using measured house-edge and true-count-frequency data.

Everything is saved automatically, so you resume exactly where you left off.
