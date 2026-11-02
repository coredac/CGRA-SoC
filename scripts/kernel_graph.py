"""Write caller-owned native kernel DAGs as Taskflow MLIR."""

from dataclasses import dataclass
import json


@dataclass
class Tensor:
    name: str
    rows: int
    columns: int
    bits: int
    state: str = ""
    stride: int = 0
    offset: int = 0

    @property
    def type(self):
        layout = (
            f", strided<[{self.stride}, 1], offset: {self.offset}>"
            if self.stride
            else ""
        )
        return f"memref<{self.rows}x{self.columns}xi{self.bits}{layout}>"

    @property
    def argument(self):
        return f"%{self.name}"

    @property
    def value(self):
        return self.state or self.argument


class Graph:
    def __init__(self, name):
        self.name = name
        self.tensors = []
        self.declarations = []
        self.tasks = []

    def tensor(self, name, rows, columns, bits):
        value = Tensor(name, rows, columns, bits)
        self.tensors.append(value)
        return value

    def view(self, name, source, row, column, rows, columns):
        stride = source.stride or source.columns
        value = Tensor(
            name,
            rows,
            columns,
            source.bits,
            stride=stride,
            offset=source.offset + row * stride + column,
        )

        def subview(result, operand):
            self.tasks.append(
                f"    {result} = memref.subview {operand}[{row}, {column}] "
                f"[{rows}, {columns}] [1, 1] : {source.type} to {value.type}"
            )

        subview(value.argument, source.argument)
        if source.state:
            value.state = f"%{name}_ready"
            subview(value.state, source.value)
        return value

    def join(self, name, target, views):
        result = f"%{name}_done"
        states = ", ".join(value.value for value in views)
        types = ", ".join(value.type for value in [target, *views])
        self.tasks.append(
            f"    {result} = taskflow.join {target.argument} [{states}] "
            f": ({types}) -> {target.type}"
        )
        target.state = result
        return target

    def call(self, name, kernel, inputs, outputs, parameters=None, narrow=False):
        tensors = [*inputs, *outputs]
        types = ", ".join(value.type for value in tensors)
        reads = ", ".join(value.value for value in inputs)
        originals = ", ".join(value.argument for value in inputs)
        writes = ", ".join(value.argument for value in outputs)
        read_types = ", ".join(value.type for value in inputs)
        write_types = ", ".join(value.type for value in outputs)
        results = [f"%{name}_done{index}" for index in range(len(outputs))]
        result_types = write_types if len(outputs) == 1 else f"({write_types})"
        attrs = f'soc.kernel = "{kernel}"'
        if narrow:
            attrs += ', soc.narrow = "saturate"'
        self.declarations.append(
            f"  func.func private @{name}_kernel({types}) attributes {{{attrs}}}"
        )
        attrs = ""
        if parameters:
            fields = ", ".join(
                f"{key} = {json.dumps(value) if isinstance(value, str) else f'{value} : i64'}"
                for key, value in parameters.items()
            )
            attrs = f"      {{soc.parameters = {{{fields}}}}}\n"
        block = ", ".join(
            f"%arg{index}: {value.type}" for index, value in enumerate(tensors)
        )
        arguments = ", ".join(f"%arg{index}" for index in range(len(tensors)))
        done = ", ".join(f"%arg{index}" for index in range(len(inputs), len(tensors)))
        self.tasks.append(f"""    {', '.join(results)} = taskflow.task @{name}
      will_reads({reads} : {read_types})
      will_writes({writes} : {write_types})
      [original_read_memrefs({originals} : {read_types}), original_write_memrefs({writes} : {write_types})]
{attrs}      : ({types}) -> {result_types} {{
    ^bb0({block}):
      func.call @{name}_kernel({arguments}) : ({types}) -> ()
      taskflow.yield done_writes({done} : {write_types})
    }}""")
        for value, result in zip(outputs, results):
            value.state = result

    def emit(self, returned):
        arguments = ", ".join(
            f"{value.argument}: {value.type}" for value in self.tensors
        )
        results = ", ".join(value.type for value in returned)
        result_types = results if len(returned) == 1 else f"({results})"
        values = ", ".join(value.value for value in returned)
        return "\n".join(
            [
                "// Generated native kernel DAG. Placement and storage are compiler-owned.",
                "module {",
                *self.declarations,
                f"  func.func @{self.name}({arguments}) -> {result_types} {{",
                *self.tasks,
                f"    return {values} : {results}",
                "  }",
                "}",
                "",
            ]
        )
