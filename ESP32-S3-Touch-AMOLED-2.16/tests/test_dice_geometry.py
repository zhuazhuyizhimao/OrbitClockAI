"""Verify the selectable dice are closed solids with the right scoring faces."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'a native C++ compiler is needed')
class DiceGeometryTests(unittest.TestCase):
    def test_closed_convex_meshes_and_numbered_face_counts(self):
        code = r'''
#include "dice_geometry.h"
#include <cassert>
#include <map>
#include <set>
#include <utility>
using namespace dice_geometry;
int main() {
  for(int sides=3;sides<=8;++sides) {
    const Mesh &m=get(sides);assert(m.scoringFaces==sides);
    assert(m.faceCount==(sides==3?5:sides));
    std::map<std::pair<int,int>,int> edges;
    unsigned scored=0;
    for(int i=0;i<m.faceCount;++i) {
      const auto &f=m.faces[i];if(f.scored)++scored;
      assert(f.count>=3&&f.count<=5&&f.labelSize>0);
      assert(std::fabs(f.normal.dot(f.normal)-1)<.0001f);
      assert(f.normal.dot(f.center)>.1f);
      assert(std::fabs(f.u.dot(f.normal))<.0001f&&std::fabs(f.v.dot(f.normal))<.0001f);
      for(int j=0;j<f.count;++j) {
        int a=f.indices[j],b=f.indices[(j+1)%f.count];assert(a!=b&&a<m.vertexCount&&b<m.vertexCount);
        if(a>b){int temp=a;a=b;b=temp;}++edges[{a,b}];
        assert(std::fabs((m.vertices[f.indices[j]]-f.center).dot(f.normal))<.0001f);
      }
      // No vertex lies outside any outward-oriented face plane.
      for(int v=0;v<m.vertexCount;++v)assert((m.vertices[v]-f.center).dot(f.normal)<.0001f);
    }
    assert(scored==sides);
    for(auto e:edges)assert(e.second==2);
    assert(int(m.vertexCount)-int(edges.size())+m.faceCount==2);
  }
  const auto &octa=get(8);assert(octa.vertexCount==6&&octa.faceCount==8);
  for(const auto &f:octa.faces)assert(f.count==3);
  // A regular octahedron has twelve edges of equal length.
  float edge=0;
  for(const auto &f:octa.faces)for(int j=0;j<3;++j) {
    const auto d=octa.vertices[f.indices[j]]-octa.vertices[f.indices[(j+1)%3]];
    if(edge==0)edge=d.dot(d);assert(std::fabs(d.dot(d)-edge)<.0001f);
  }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path/'check.cpp').write_text(code)
            subprocess.run([COMPILER, '-std=c++17', '-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'), '-o', str(path/'check')],check=True,capture_output=True)
            subprocess.run([str(path/'check')],check=True,timeout=20,capture_output=True)
