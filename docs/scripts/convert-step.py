"""Convert the supplied EasyEDA STEP into a colored GLB using OpenCascade.
Usage: python convert-step.py source.step output.glb
Requires cadquery-ocp. Never edits the source STEP.
"""
import sys
from pathlib import Path
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString, TCollection_AsciiString
from OCP.XCAFDoc import XCAFDoc_DocumentTool
from OCP.collections import Sequence_TDF_Label as TDF_LabelSequence
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.RWGltf import RWGltf_CafWriter
from OCP.collections import IndexedDataMap_TCollection_AsciiString_TCollection_AsciiString as TColStd_IndexedDataMapOfStringString
from OCP.Message import Message_ProgressRange
from OCP.IFSelect import IFSelect_RetDone
source, target = sys.argv[1:]
doc = TDocStd_Document(TCollection_ExtendedString('OSRCORE'))
reader = STEPCAFControl_Reader()
reader.SetColorMode(True)
reader.SetNameMode(True)
if reader.ReadFile(source) != IFSelect_RetDone or not reader.Transfer(doc):
    raise RuntimeError('STEP import failed')
tool = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
labels = TDF_LabelSequence()
tool.GetFreeShapes(labels)
for i in range(1, labels.Length()+1):
    shape = tool.GetShape_s(labels.Value(i))
    mesh = BRepMesh_IncrementalMesh(shape, 0.08, False, 0.3, True)
    if not mesh.IsDone(): raise RuntimeError('Meshing failed')
writer = RWGltf_CafWriter(TCollection_AsciiString(target), True)
if not writer.Perform(doc, TColStd_IndexedDataMapOfStringString(), Message_ProgressRange()):
    raise RuntimeError('GLB export failed')
if Path(target).stat().st_size < 10000: raise RuntimeError('Unexpectedly small model')
print({'roots': labels.Length(), 'bytes': Path(target).stat().st_size, 'output': target})
