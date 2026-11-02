"""Check native tile-query domains, access windows, and shared workspace rules."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEMMINI = ROOT / "chipyard/generators/gemmini/software/gemmini-rocc-tests"

SOURCE = r"""
#include "Backend/SoC/Query.h"
#include "workspace.h"
#include "gemmini.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static uint64_t value(const amoeba_query_result_t *r, const char *name) {
  for (unsigned i=0; i<r->parameter_count; ++i)
    if (!strcmp(r->parameters[i].name,name)) return r->parameters[i].value;
  return UINT64_MAX;
}

static int matrix(amoeba_query_fn query) {
  amoeba_query_parameter_t p[] = {{"m",16},{"n",768},{"k",3072},{"transpose_a",0},{"transpose_b",0}};
  amoeba_query_view_t inputs[] = {{0,16,3072,3072,1},{0,3072,768,768,1},{0,1,768,3072,4}};
  amoeba_query_view_t output = {0,16,768,3072,4};
  amoeba_query_call_t call = {AMOEBA_QUERY_ABI,5,3,1,p,inputs,&output};
  amoeba_query_result_t r;
  if (query(&call,NULL,&r) || !r.legal || r.rank!=3 || r.axes[2].kind!=AMOEBA_QUERY_REDUCTION || r.axes[2].extent!=3072 || r.axes[0].alignment!=DIM) return 1;
  uint64_t origin[]={0,0,0}, extent[]={16,768,3072};
  amoeba_query_tile_t tile={3,origin,extent};
  if (query(&call,&tile,&r) || r.legal || !r.error) return 2;
  extent[1]=64; extent[2]=768;
  if (query(&call,&tile,&r) || !r.legal || !r.inputs[2].enabled || r.outputs[0].enabled || value(&r,"accumulate")!=0 || value(&r,"store")!=0) return 3;
  gemmini_workspace_t w=gemmini_matrix_workspace(16,768,64);
  if (r.scratch_bytes!=w.scratch_rows*DIM || r.accumulator_bytes!=w.accumulator_rows*DIM*sizeof(acc_t) || r.spm_bytes!=(uint64_t)BANK_NUM*BANK_ROWS*DIM) return 4;
  origin[1]=64; origin[2]=768;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[2].enabled || r.outputs[0].enabled || r.inputs[0].view.offset!=768 || r.inputs[1].view.offset!=768*768+64 ||
      r.outputs[0].view.offset!=64*4 || value(&r,"output_stride")!=768*4) return 5;
  origin[2]=2304;
  if (query(&call,&tile,&r) || !r.legal || !r.outputs[0].enabled || value(&r,"store")!=1) return 6;
  p[4].value=1; inputs[1]=(amoeba_query_view_t){0,768,3072,3072,1};
  if (query(&call,&tile,&r) || !r.legal || r.inputs[1].view.offset!=64*3072+2304 || r.inputs[1].view.rows!=64 || r.inputs[1].view.columns!=768) return 7;
  p[3].value=1;
  if (!query(&call,&tile,&r)) return 8;
  p[3].value=0; p[4].value=0;
  p[0].value=2; p[1].value=10; p[2].value=17;
  inputs[0]=(amoeba_query_view_t){32,2,17,64,1};
  inputs[1]=(amoeba_query_view_t){0,17,10,32,1};
  inputs[2]=(amoeba_query_view_t){0,1,10,40,4};
  output=(amoeba_query_view_t){0,2,10,80,4};
  origin[0]=origin[1]=0; origin[2]=16;
  extent[0]=2; extent[1]=10; extent[2]=1;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.offset!=48 || r.inputs[0].view.row_stride!=64 || r.outputs[0].view.row_stride!=80) return 9;
  origin[2]=1;
  if (query(&call,&tile,&r) || r.legal) return 10;
  call.abi=0;
  if (!query(&call,NULL,&r) || r.abi!=AMOEBA_QUERY_ABI) return 11;
  return 0;
}

static int conv(amoeba_query_fn query) {
  amoeba_query_parameter_t p[]={{"input_rows",32},{"input_columns",32},{"input_channels",3},{"output_rows",32},{"output_columns",32},{"output_channels",16},
    {"kernel",3},{"stride",1},{"dilation",1},{"padding_top",1},{"padding_left",1}};
  amoeba_query_view_t inputs[]={{0,1024,3,3,1},{0,27,16,16,1}};
  amoeba_query_view_t output={0,1024,16,16,1};
  amoeba_query_call_t call={AMOEBA_QUERY_ABI,11,2,1,p,inputs,&output};
  amoeba_query_result_t r;
  if (query(&call,NULL,&r) || !r.legal || r.rank!=1 || r.axes[0].extent!=32) return 1;
  uint64_t origin=0, extent=32;
  amoeba_query_tile_t tile={1,&origin,&extent};
  if (query(&call,&tile,&r) || r.legal || r.accumulator_bytes!=1024*DIM*sizeof(acc_t)) return 2;
  extent=2;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=3*32 || r.inputs[0].view.offset || value(&r,"input_rows")!=3 || value(&r,"padding_top")!=1) return 3;
  origin=2;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=4*32 || r.inputs[0].view.offset!=32*3 || value(&r,"padding_top")!=0 || r.outputs[0].view.offset!=2*32*16) return 4;
  origin=30;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=3*32 || r.inputs[0].view.offset!=29*32*3) return 5;
  p[3].value=p[4].value=16; p[7].value=2; p[9].value=p[10].value=0;
  output=(amoeba_query_view_t){0,256,16,16,1};
  origin=2;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=5*32 || r.inputs[0].view.offset!=4*32*3 || r.outputs[0].view.offset!=2*16*16) return 6;
  inputs[0].row_stride=4;
  if (!query(&call,&tile,&r)) return 7;
  for (unsigned rows=1; rows<=16; rows*=2)
    for (unsigned channels=3; channels<=48; channels*=2)
      for (unsigned stride=1; stride<=2; ++stride)
        for (unsigned dilation=1; dilation<=2; ++dilation) {
          gemmini_workspace_t w=gemmini_conv_workspace(rows,16,channels,32,3,stride,dilation);
          int scratch=tiled_conv_total_spad_rows(false,stride,1,dilation,false,false,false,1,rows,16,32,3,3,channels,1,1);
          int acc=tiled_conv_total_spad_rows(true,stride,1,dilation,false,false,false,1,rows,16,32,3,3,channels,1,1);
          if (w.scratch_rows!=(unsigned)scratch || w.accumulator_rows!=(unsigned)acc) return 8;
        }
  return 0;
}

static int copy(amoeba_query_fn query) {
  amoeba_query_view_t source={32,40,60,80,1}, destination={64,40,60,96,1};
  amoeba_query_call_t call={AMOEBA_QUERY_ABI,0,1,1,NULL,&source,&destination};
  amoeba_query_result_t r;
  if (query(&call,NULL,&r) || !r.legal || r.rank!=2 || r.axes[0].extent!=40 || r.axes[1].extent!=60) return 1;
  uint64_t origin[]={0,0}, extent[]={40,60};
  amoeba_query_tile_t tile={2,origin,extent};
  if (query(&call,&tile,&r) || r.legal) return 2;
  origin[0]=16; origin[1]=32; extent[0]=8; extent[1]=16;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.offset!=32+16*80+32 || r.outputs[0].view.offset!=64+16*96+32 ||
      value(&r,"source_stride")!=80 || value(&r,"destination_stride")!=96 || r.scratch_bytes!=8*DIM || r.accumulator_bytes) return 3;
  origin[0]=origin[1]=0; extent[0]=2; extent[1]=DIM*MAX_BLOCK_LEN-1;
  source=(amoeba_query_view_t){32,40,DIM*MAX_BLOCK_LEN, DIM*(MAX_BLOCK_LEN+1),1};
  destination=(amoeba_query_view_t){64,40,DIM*MAX_BLOCK_LEN,DIM*(MAX_BLOCK_LEN+2),1};
  if (query(&call,&tile,&r) || !r.legal || value(&r,"columns")!=DIM*MAX_BLOCK_LEN-1 ||
      r.scratch_bytes!=((MAX_BLOCK_LEN-1)*DIM+2)*DIM || r.accumulator_bytes) return 19;
  extent[1]=DIM*MAX_BLOCK_LEN+1;
  source.columns=destination.columns=extent[1];
  if (query(&call,&tile,&r) || r.legal) return 20;
  origin[0]=16; origin[1]=32; extent[0]=8; extent[1]=16;
  source=(amoeba_query_view_t){32,40,60,256,4}; destination=(amoeba_query_view_t){64,40,60,320,4};
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.offset!=32+16*256+32*4 || r.outputs[0].view.offset!=64+16*320+32*4 ||
      value(&r,"element_bytes")!=4 || r.scratch_bytes || r.accumulator_bytes!=8*DIM*4) return 4;
  origin[0]=origin[1]=0; extent[0]=2; extent[1]=DIM+1;
  if (query(&call,&tile,&r) || r.legal) return 21;
  origin[0]=16; origin[1]=32; extent[0]=8; extent[1]=16;
  destination.row_stride=239;
  if (!query(&call,&tile,&r)) return 5;
  destination.row_stride=320; destination.element_bytes=1;
  if (!query(&call,&tile,&r)) return 6;
  source=(amoeba_query_view_t){32,1,2976,2976,1}; destination=(amoeba_query_view_t){64,1,2976,2976,1};
  if (query(&call,NULL,&r) || !r.legal || r.rank!=1 || strcmp(r.axes[0].name,"elements") || r.axes[0].extent!=2976 || r.axes[0].alignment!=DIM) return 7;
  tile.rank=1; origin[0]=0; extent[0]=176;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=11 || r.inputs[0].view.columns!=16 || r.inputs[0].view.offset!=32 ||
      r.outputs[0].view.offset!=64 || value(&r,"rows")!=11 || value(&r,"columns")!=16 || value(&r,"source_stride")!=16 || value(&r,"destination_stride")!=16) return 8;
  origin[0]=176;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.offset!=32+176 || r.outputs[0].view.offset!=64+176) return 9;
  origin[0]=2816; extent[0]=160;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=10 || r.inputs[0].view.columns!=16 || r.inputs[0].view.offset!=32+2816) return 10;
  origin[0]=0; extent[0]=DIM*DIM+DIM;
  if (query(&call,&tile,&r) || r.legal) return 11;
  source=(amoeba_query_view_t){32,1,2983,2983,1}; destination=(amoeba_query_view_t){64,1,2983,2983,1};
  origin[0]=2976; extent[0]=7;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.rows!=1 || r.inputs[0].view.columns!=7 || r.inputs[0].view.row_stride!=7 ||
      r.inputs[0].view.offset!=32+2976 || r.outputs[0].view.offset!=64+2976) return 12;
  origin[0]=0; extent[0]=2983;
  if (query(&call,&tile,&r) || r.legal) return 13;
  origin[0]=1; extent[0]=16;
  if (query(&call,&tile,&r) || r.legal) return 14;
  source=(amoeba_query_view_t){32,1,177,177,1}; destination=(amoeba_query_view_t){64,1,177,177,1};
  origin[0]=0; extent[0]=177;
  if (query(&call,&tile,&r) || r.legal) return 15;
  source=(amoeba_query_view_t){32,1,2976,2976*4,4}; destination=(amoeba_query_view_t){64,1,2976,2976*4,4};
  origin[0]=176; extent[0]=176;
  if (query(&call,&tile,&r) || !r.legal || r.inputs[0].view.offset!=32+176*4 || r.outputs[0].view.offset!=64+176*4 ||
      value(&r,"element_bytes")!=4 || value(&r,"source_stride")!=16*4 || value(&r,"destination_stride")!=16*4 || r.accumulator_bytes!=11*DIM*4) return 16;
  destination.row_stride+=4;
  if (query(&call,NULL,&r) || !r.legal || r.rank!=2 || r.axes[0].extent!=1 || r.axes[1].extent!=2976) return 17;
  return 0;
}

int main(int argc, char **argv) {
  if (argc!=2) return 1;
  void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
  if (!library) { fprintf(stderr,"%s\n",dlerror()); return 2; }
  amoeba_query_fn matrix_query=(amoeba_query_fn)dlsym(library,"amoeba_query_matrix");
  amoeba_query_fn conv_query=(amoeba_query_fn)dlsym(library,"amoeba_query_conv");
  amoeba_query_fn copy_query=(amoeba_query_fn)dlsym(library,"amoeba_query_copy");
  if (!matrix_query || !conv_query || !copy_query) return 3;
  int result=matrix(matrix_query);
  if (result) { fprintf(stderr,"matrix query check %d failed\n",result); return 4; }
  result=conv(conv_query);
  if (result) { fprintf(stderr,"Conv query check %d failed\n",result); return 5; }
  result=copy(copy_query);
  if (result) { fprintf(stderr,"copy query check %d failed\n",result); return 6; }
  dlclose(library);
  return 0;
}
"""


class RuntimeQueryTest(unittest.TestCase):
    def test_native_queries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            flags = [
                "cc",
                "-std=gnu11",
                "-O2",
                "-fsanitize=undefined",
                "-fno-sanitize=shift-base",
                "-fno-sanitize-recover=all",
                "-I",
                str(ROOT / "amoeba/include"),
                "-I",
                str(ROOT / "runtime/planning"),
                "-I",
                str(GEMMINI / "include"),
                "-I",
                str(GEMMINI),
            ]
            library = path / "native-query.so"
            subprocess.run(
                [
                    *flags,
                    "-fPIC",
                    "-shared",
                    str(ROOT / "runtime/planning/query.c"),
                    "-o",
                    str(library),
                ],
                check=True,
            )
            source = path / "test.c"
            source.write_text(SOURCE)
            binary = path / "test"
            subprocess.run([*flags, str(source), "-ldl", "-o", str(binary)], check=True)
            subprocess.run([str(binary), str(library)], check=True)


if __name__ == "__main__":
    unittest.main()
