from pathlib import Path

from trivia_pack.blacklist import Blacklist


def test_keyword_match_drops_question(tmp_path: Path) -> None:
    f = tmp_path / "bl.txt"
    f.write_text("NFL\nSuper Bowl\n# comment\n\nroyal\n", encoding="utf-8")
    bl = Blacklist.from_file(f)

    assert bl.is_blacklisted("Who won Super Bowl XII?", "Cowboys") is True
    assert bl.is_blacklisted("Capital of France?", "Paris") is False
    # case insensitive
    assert bl.is_blacklisted("the nfl is popular", "?") is True
    # word boundary — "rfl" in the middle of a word should NOT match "NFL"
    assert bl.is_blacklisted("trflactor", "?") is False


def test_match_in_answer_also_drops(tmp_path: Path) -> None:
    f = tmp_path / "bl.txt"
    f.write_text("Tudor\n", encoding="utf-8")
    bl = Blacklist.from_file(f)
    assert bl.is_blacklisted("Which dynasty?", "The Tudor dynasty") is True


def test_empty_file_drops_nothing(tmp_path: Path) -> None:
    f = tmp_path / "bl.txt"
    f.write_text("", encoding="utf-8")
    bl = Blacklist.from_file(f)
    assert bl.is_blacklisted("anything goes", "any answer") is False


def test_comment_and_empty_lines_ignored(tmp_path: Path) -> None:
    f = tmp_path / "bl.txt"
    f.write_text("# a comment\n\n   \nNBA\n", encoding="utf-8")
    bl = Blacklist.from_file(f)
    assert bl.size == 1
    assert bl.is_blacklisted("the NBA finals", "anything") is True


def test_multi_word_phrase_matches(tmp_path: Path) -> None:
    f = tmp_path / "bl.txt"
    f.write_text("Premier League\n", encoding="utf-8")
    bl = Blacklist.from_file(f)
    assert bl.is_blacklisted("Premier League winner?", "Liverpool") is True
    # individual words should not match alone
    assert bl.is_blacklisted("the premier of china", "?") is False
    assert bl.is_blacklisted("league of nations", "?") is False
