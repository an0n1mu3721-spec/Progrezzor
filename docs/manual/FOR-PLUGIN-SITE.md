# Note til plugin-site-projektet (music.henrikjuhl.dk)

Progrezzor har en manual, der skal ligge som egen side på sitet: `/plugins/progrezzor/manual/`.
Denne mappe må kun læses herfra. Ret ikke i Progrezzor-projektet.

## Filer
- `manual.md`: ren Markdown med et lille frontmatter (title, description), overskrifter, tabeller, backtick-taster og billeder som `![tekst](img/navn.png)`. Ingen HTML.
- `img/*.png`: 31 billeder (det store oversigtsbillede er 1196 px bredt). Mørk baggrund indbygget, så de passer til sitets mørke look.

## Det, sitet skal kunne
1. Kopiér `manual.md` og `img/` til `content/plugins/progrezzor/manual/` (eller lign.) og vis den som en side. Plugin-siden skal have et tydeligt link til den, og sitemap skal have siden med.
2. `react-markdown` vælger ikke tabeller uden `remark-gfm`. Manualen bruger tabeller, så den skal tilføjes. Interne links som `#into-your-daw` kræver id'er på overskrifter (fx `rehype-slug`).
3. `scripts/copy-content-assets.mjs` kopierer kun filer direkte i plugin-mappen. Billederne ligger i en undermappe, så scriptet skal gå rekursivt. PNG'erne konverteres allerede til WebP. Brug lossy kvalitet 90, hvis de er over 100 KB, som scriptet allerede gør.
4. Billederne skal have `alt` (det er billedteksten i Markdown), brede billeder skal skalere til sidens bredde, og siden skal fungere på mobil. Oversigtsbilledet har små tal, så lad det kunne åbnes i fuld størrelse (klik).
5. Ingen eksterne tjenester, ingen varemærker i teksten ud over DAW-navnene, der står i manualens DAW-afsnit.

## Opdatering
Når manualen ændres i Progrezzor, kopieres de to ting igen. Intet andet er koblet sammen.
