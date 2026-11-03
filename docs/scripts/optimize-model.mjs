// Preserve the CAD shape and colors; batch static faces by material and compress.
import fs from "node:fs/promises";
import { NodeIO, PropertyType } from "@gltf-transform/core";
import { ALL_EXTENSIONS } from "@gltf-transform/extensions";
import {
  dedup,
  flatten,
  join,
  weld,
  meshopt,
  getBounds,
} from "@gltf-transform/functions";
import { MeshoptEncoder, MeshoptDecoder } from "meshoptimizer";
const input = process.argv[2],
  output = process.argv[3];
if (!input || !output)
  throw Error("Usage: node scripts/optimize-model.mjs input.glb output.glb");
await MeshoptEncoder.ready;
const io = new NodeIO()
  .registerExtensions(ALL_EXTENSIONS)
  .registerDependencies({
    "meshopt.encoder": MeshoptEncoder,
    "meshopt.decoder": MeshoptDecoder,
  });
const doc = await io.read(input);
const stats = () => ({
  meshes: doc.getRoot().listMeshes().length,
  primitives: doc
    .getRoot()
    .listMeshes()
    .reduce((n, m) => n + m.listPrimitives().length, 0),
  materials: doc.getRoot().listMaterials().length,
  bounds: getBounds(doc.getRoot().listScenes()[0]),
});
const before = stats();
await doc.transform(
  dedup({ propertyTypes: [PropertyType.MATERIAL] }),
  flatten(),
  join({ keepNamed: false }),
  weld(),
  meshopt({
    encoder: MeshoptEncoder,
    level: "high",
    quantizePosition: 16,
    quantizeNormal: 12,
  }),
);
await io.write(output, doc);
const result = {
  before,
  after: stats(),
  inputBytes: (await fs.stat(input)).size,
  outputBytes: (await fs.stat(output)).size,
};
await fs.writeFile(
  output + ".stats.json",
  JSON.stringify(result, null, 2) + "\n",
);
console.log(JSON.stringify(result, null, 2));
