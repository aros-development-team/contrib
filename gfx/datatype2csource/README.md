# DatatypeToCSource

DatatypeToCSource loads picture files through `datatypes.library` and writes C source to standard output. The generated data can be used with Rawimage MCC.

Usage:

```text
DatatypeToCSource file1.png file2.png
DatatypeToCSource ram:#?.png PACK
```

`PACK` compresses the image data with bzip2.
