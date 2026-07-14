# T9 data files
* Tier 1 — function words
* Tier 2 — lemma_list.txt
* Tier 3A — chat slang
* Tier 3B — fillers
* Tier 4 — formal discourse

For context-gated patterns, we keep the data model simple: 
1) scored bigrams
2) parameterized frames
3) small token-sets for frame expansion
4) gating rules that map context signals (mode, punctuation, sentence length, question mark) to boosts/enabling.
