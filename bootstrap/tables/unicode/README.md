# Unicode table provenance

Marimay language version `0.1-design` pins Unicode 17.0.0 identifier properties under D023.

Input:

- file: `DerivedCoreProperties-17.0.0.txt`;
- source: <https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt>;
- published header date: 2025-07-30;
- SHA-256: `24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08`;
- terms: <https://www.unicode.org/terms_of_use.html>.

Additional formatter-width input under D026:

- file: `EastAsianWidth-17.0.0.txt`;
- source: <https://www.unicode.org/Public/17.0.0/ucd/EastAsianWidth.txt>;
- SHA-256: `ea7ce50f3444a050333448dffef1cadd9325af55cbb764b4a2280faf52170a33`;
- property contract: Unicode 17.0.0 UAX #11, <https://www.unicode.org/reports/tr11/tr11-44.html>.

`tools/generate-unicode` reads `XID_Start`, `XID_Continue`, `Grapheme_Extend`, and `Default_Ignorable_Code_Point` from the core-property input and `W`/`F` from the East Asian Width input. It regenerates committed identifier and formatter-width headers. Release builds consume those headers directly and never run the generator or access the network.
