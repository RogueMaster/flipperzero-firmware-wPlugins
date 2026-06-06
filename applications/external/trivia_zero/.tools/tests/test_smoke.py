from trivia_pack import __version__


def test_package_version_exposed() -> None:
    assert __version__ == "0.1.0"
