from __future__ import annotations

import pytest

from trivia_pack.mc_phrasing import is_multiple_choice_phrasing


@pytest.mark.parametrize(
    "question",
    [
        "Which of the following European languages is a language isolate?",
        "Which of the following Japanese islands is the biggest?",
        "Which of these countries is not a UN member state?",
        "Which of these places is a location in Cornwall?",
        "Which one of these countries borders with Poland?",
        "Which one of the following is NOT a Greek god?",
        "Of the following, which Stephen King novel was published earliest?",
        "Which is the largest of these 4 islands?",
        "Which of those statements is false?",
        # Extra word between "the" and "following".
        "The mountainous Khyber Pass connects which of the two following countries?",
        # Spanish (with and without accent on "cual").
        "Cual de los siguientes paises NO esta en Europa?",
        "Cuál de las siguientes peliculas gano el Oscar en 1995?",
        "Cual de estas peliculas NO conto con la participacion de John Candy?",
        "Cuál de estos animales es un mamifero?",
        "El paso de Jiber conecta cual de los dos siguientes paises?",
        "De los siguientes presidentes, quien sirvio mas tiempo?",
        # Negative-form MC questions: OpenTDB capitalizes NOT/NO.
        "Which Beatles album does NOT feature any of the band members on its cover?",
        "Which movie did NOT feature the late actor John Candy?",
        "Which song is NOT by singer Tom Jones?",
        "Cual album de The Beatles NO presenta a ningun miembro de la banda?",
        "Cual cancion NO es de Tom Jones?",
        "Cual nacion NO tuvo una colonia en la America del Norte moderna?",
    ],
)
def test_drops_choice_list_phrasing_in_question(question: str) -> None:
    assert is_multiple_choice_phrasing(question, "Some answer") is True


@pytest.mark.parametrize(
    "answer",
    [
        "All of the above",
        "None of the above",
        "All of these songs",
        "None of these",
        "Todas las anteriores",
        "Ninguna de las anteriores",
        "Ninguno de los anteriores",
    ],
)
def test_drops_answer_referencing_choice_list(answer: str) -> None:
    assert is_multiple_choice_phrasing("A neutral question?", answer) is True


@pytest.mark.parametrize(
    ("question", "answer"),
    [
        ("Capital of Spain?", "Madrid"),
        ("Who painted the Mona Lisa?", "Leonardo da Vinci"),
        ("In what year did WWII end?", "1945"),
        # "these" / "those" alone, without the "of these/those/the above" frame,
        # must NOT trip the filter — the question is self-contained.
        ("What do these symbols represent in mathematics?", "Set membership"),
        # Spanish self-contained questions must survive.
        ("Cual es la capital de Espana?", "Madrid"),
        ("Quien pinto la Mona Lisa?", "Leonardo da Vinci"),
        # Lowercase "no"/"not" inside normal prose must NOT trigger the
        # case-sensitive negative-MC filter — those reserved for ALL CAPS.
        ("Did the Roman Empire not extend to Britain?", "It did"),
        ("Una respuesta corta no afirmativa cuenta?", "No"),
    ],
)
def test_keeps_self_contained_questions(question: str, answer: str) -> None:
    assert is_multiple_choice_phrasing(question, answer) is False


def test_match_is_case_insensitive() -> None:
    assert is_multiple_choice_phrasing("WHICH OF THE FOLLOWING is true?", "X") is True
    assert is_multiple_choice_phrasing("Which Of These films won?", "X") is True
