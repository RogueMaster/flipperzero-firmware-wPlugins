# Prime Factorization

A small educational utility that breaks an integer into its prime factors. It works using plain trial division, with the highest number possible being 2,147,483,647 due to flipper's limitation.

What happens when you input 0 or 1? Only one way to find out.
(It doesn't let you put 0)

```
360 = 2 x 2 x 2 x 3 x 3 x 5
100 = 2 x 2 x 5 x 5
17  = 17
```

## How it works

Plain trial division:

1. Start with divisor `2`.
2. While the divisor divides the number evenly, record it and divide the number by it.
3. Increment the divisor and continue while `divisor * divisor <= remaining number`.
4. If the remaining number is greater than `1`, it is the last prime factor.
