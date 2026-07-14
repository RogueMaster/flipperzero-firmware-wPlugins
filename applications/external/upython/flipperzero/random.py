def random() -> float:
    '''
    Get a random float value between 0.0 (inclusive) and 1.0 (exclusive).

    :returns: The random value.

    .. versionadded:: 1.6.0
    '''
    pass

def randrange(start: int, stop: int, step: int = 1) -> int:
    '''
    Get a random integer between ``start`` (inclusive) and ``stop`` 
    (exclusive) with an optional ``step`` between the values.

    :param start: The start value.
    :param stop: The end value.
    :param step: The optional step value.
    :returns: The random value.

    .. versionadded:: 1.6.0

    .. hint::

        This function does only generate integer values.
    '''
    pass

def randint(a: int, b: int) -> int:
    '''
    Get a random integer between ``a`` (inclusive) and ``b`` (inclusive).

    :param a: The lower value.
    :param b: The upper value.
    :returns: The random value.

    .. versionadded:: 1.6.0
    '''
    pass

def choice[T](seq: list[T]) -> T:
    '''
    Get a random element from the provided sequence.

    :param seq: The sequence to use.
    :returns: A random element.

    .. versionadded:: 1.6.0
    '''
    pass

def getrandbits(k: int) -> int:
    '''
    Get ``k`` random bits. 

    :param k: The number of bits.
    :returns: The random bits.

    .. versionadded:: 1.6.0
    '''
    pass

def uniform(a: float, b: float) -> float:
    '''
    Get a random float value between ``a`` (inclusive) and ``b`` (inclusive).

    :param a: The lower value.
    :param b: The upper value.
    :returns: The random value.

    .. versionadded:: 1.6.0
    '''
    pass

def seed(a: int) -> None:
    '''
    Initialize the random number generator. 

    :param a: The initialization value to use.

    .. versionadded:: 1.6.0

    .. hint::

        Random generator seeding is done automatically, so there is no need to call this function.
    '''
    pass
