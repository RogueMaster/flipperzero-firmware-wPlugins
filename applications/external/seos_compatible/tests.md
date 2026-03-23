| keys     | credential keys | reader supports | result   |
| -------- | --------------- | --------------- | -------- |
| standard | mob             | mob, standard   | mob      |
| standard | mob             | standard        | <fail>   | 
| standard | -               | mob, standard   | standard |
| zero     | mob             | mob, standard   | mob      |
| zero     | -               | standard        | <fail>   |
| zero     | mob             | standard        | <fail>   |



2 readers: standard and mob + standard
2 credentials: one with no keys, one with mob keys


----

