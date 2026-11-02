// Causal Softmax from existing native kernels. One complete four-row tile.
module {
  func.func private @max(memref<4x4xi32>, memref<4x1xi32>) attributes {soc.kernel = "attention_max"}
  func.func private @exp(memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi32>, memref<4x1xi32>) attributes {soc.kernel = "attention_exp"}
  func.func private @norm(memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi8>) attributes {soc.kernel = "attention_norm", soc.narrow = "saturate"}
  func.func @softmax(%scores: memref<4x4xi32>, %max: memref<4x1xi32>, %exp: memref<4x4xi32>, %sum: memref<4x1xi32>, %output: memref<4x4xi8>) -> (memref<4x1xi32>, memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi8>) {
    %max_done = taskflow.task @max
      will_reads(%scores : memref<4x4xi32>)
      will_writes(%max : memref<4x1xi32>)
      [original_read_memrefs(%scores : memref<4x4xi32>), original_write_memrefs(%max : memref<4x1xi32>)]
      {soc.parameters = {columns = "n", row = 0 : i64}}
      : (memref<4x4xi32>, memref<4x1xi32>) -> memref<4x1xi32> {
    ^bb0(%arg0: memref<4x4xi32>, %arg1: memref<4x1xi32>):
      func.call @max(%arg0, %arg1) : (memref<4x4xi32>, memref<4x1xi32>) -> ()
      taskflow.yield done_writes(%arg1 : memref<4x1xi32>)
    }
    %exp_done, %sum_done = taskflow.task @exp
      will_reads(%scores, %max_done : memref<4x4xi32>, memref<4x1xi32>)
      will_writes(%exp, %sum : memref<4x4xi32>, memref<4x1xi32>)
      [original_read_memrefs(%scores, %max : memref<4x4xi32>, memref<4x1xi32>), original_write_memrefs(%exp, %sum : memref<4x4xi32>, memref<4x1xi32>)]
      {soc.parameters = {columns = "n", row = 0 : i64}}
      : (memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi32>, memref<4x1xi32>) -> (memref<4x4xi32>, memref<4x1xi32>) {
    ^bb0(%arg0: memref<4x4xi32>, %arg1: memref<4x1xi32>, %arg2: memref<4x4xi32>, %arg3: memref<4x1xi32>):
      func.call @exp(%arg0, %arg1, %arg2, %arg3) : (memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi32>, memref<4x1xi32>) -> ()
      taskflow.yield done_writes(%arg2, %arg3 : memref<4x4xi32>, memref<4x1xi32>)
    }
    %output_done = taskflow.task @norm
      will_reads(%exp_done, %sum_done : memref<4x4xi32>, memref<4x1xi32>)
      will_writes(%output : memref<4x4xi8>)
      [original_read_memrefs(%exp, %sum : memref<4x4xi32>, memref<4x1xi32>), original_write_memrefs(%output : memref<4x4xi8>)]
      {soc.parameters = {columns = "n"}}
      : (memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi8>) -> memref<4x4xi8> {
    ^bb0(%arg0: memref<4x4xi32>, %arg1: memref<4x1xi32>, %arg2: memref<4x4xi8>):
      func.call @norm(%arg0, %arg1, %arg2) : (memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi8>) -> ()
      taskflow.yield done_writes(%arg2 : memref<4x4xi8>)
    }
    return %max_done, %exp_done, %sum_done, %output_done : memref<4x1xi32>, memref<4x4xi32>, memref<4x1xi32>, memref<4x4xi8>
  }
}
