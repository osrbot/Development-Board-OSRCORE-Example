import fs from "node:fs/promises";
import { createHighlighter } from "shiki";
const highlighter = await createHighlighter({
  themes: ["github-light"],
  langs: ["c"],
});
const map = JSON.parse(await fs.readFile("i18n-map.json", "utf8"));
const dirs = (await fs.readdir("..")).filter((x) => /^\d\d_/.test(x)).sort();
const source = {
  zh: await fs.readFile("zh/tutorial.md", "utf8"),
  en: await fs.readFile("tutorial.md", "utf8"),
};
const courses = [];
for (const chapter of map.chapters) {
  const n = Number(chapter.id),
    dir = dirs.find((x) => x.startsWith(chapter.id + "_"));
  const item = { id: chapter.id, dir: dir || "", zh: {}, en: {}, files: [] };
  for (const locale of ["zh", "en"]) {
    const re =
      locale === "zh"
        ? /^##\s+第\s*(\d+)\s*章[^\n]*$/gm
        : /^##\s+Chapter\s+(\d+)\s*:[^\n]*$/gm;
    const matches = [...source[locale].matchAll(re)];
    const i = matches.findIndex((x) => Number(x[1]) === n);
    const body = source[locale].slice(matches[i].index, matches[i + 1]?.index);
    const section = body.split(/^### /m);
    const intro =
      section
        .find((x) => new RegExp(`^${n}\\.2`).test(x))
        ?.replace(/^[^\n]*\n/, "")
        .trim() || "";
    const points =
      section
        .find((x) => new RegExp(`^${n}\\.1`).test(x))
        ?.split("\n")
        .filter((x) => x.startsWith("- "))
        .map((x) => x.slice(2)) || [];
    item[locale] = {
      title: chapter[locale + "Title"].replace(/^.*?[:：]\s*/, "").replace(/^\d+\s*·\s*/, ""),
      intro,
      points,
    };
  }
  if (dir) {
    for (const f of (await fs.readdir("../" + dir + "/main"))
      .filter((x) => /\.(c|h)$/.test(x))
      .sort()) {
      const code = await fs.readFile("../" + dir + "/main/" + f, "utf8");
      const start = code.indexOf("void app_main(void)");
      const snippet =
        start >= 0
          ? code.slice(start).split("\n").slice(0, 22).join("\n")
          : code.split("\n").slice(0, 22).join("\n");
      const url = `/code/${dir}/${f}.json`;
      await fs.mkdir(`public/code/${dir}`, { recursive: true });
      await fs.writeFile(
        `public${url}`,
        JSON.stringify({
          code,
          html: highlighter.codeToHtml(code, {
            lang: "c",
            theme: "github-light",
          }),
          snippet: highlighter.codeToHtml(snippet, {
            lang: "c",
            theme: "github-light",
          }),
          excerpt: start >= 0 ? "app_main" : "source",
        }),
      );
      item.files.push({ name: f, url });
    }
  }
  item.files.sort((a, b) =>
    a.name === "main.c"
      ? -1
      : b.name === "main.c"
        ? 1
        : a.name.localeCompare(b.name),
  );
  courses.push(item);
}
await fs.writeFile(".vitepress/theme/courses.json", JSON.stringify(courses));
console.log(
  "Learning data: " +
    courses.length +
    " bilingual lessons; source code loaded on demand from examples.",
);

highlighter.dispose();
