## Protocol format

```
[TYPE]      # 4 bytes e.g. .MPG, .MSG, .FIL .NIL
[LENGTH]    # 8 bytes (len(TYPE + LENGTH + CONTENT))
[CONTENT]
```

### put file
```
[CONTENT]:
PUT [filename]
```

### get file
```
[CONTENT]:
GET [filename]
```

### play mpg
```
RES [width]x[height]
```