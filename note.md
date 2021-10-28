## Protocol format

```
[TYPE]      # 4 bytes e.g. .MPG, .MSG, .FIL
[LENGTH]    # 8 bytes (len(TYPE + LENGTH + CONTENT))
[CONTENT]
```