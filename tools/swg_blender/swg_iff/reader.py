"""SWG IFF binary reader — port of sharedFile/Iff.cpp navigation semantics."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Iterator, Optional

from swg_iff.tags import TAG_FORM, tag_to_str


class IffError(Exception):
    """IFF parse or navigation error."""


@dataclass
class IffBlock:
    """One FORM or chunk at the current parse level."""

    tag: int
    length: int
    offset: int  # start of tag in file
    data_offset: int  # start of payload (after tag+length[+formtype])
    is_form: bool
    form_type: Optional[int] = None

    @property
    def tag_str(self) -> str:
        return tag_to_str(self.tag)

    @property
    def form_type_str(self) -> str:
        if self.form_type is None:
            return ""
        return tag_to_str(self.form_type)


@dataclass
class _StackFrame:
    start: int
    length: int
    used: int = 0
    in_chunk: bool = False


class IffReader:
    """
    Read-only IFF navigator.

    Block headers: tag and length are big-endian (ntohl in C++).
    Chunk payload scalars: little-endian (native memcpy in C++ read_int32/read_float).
    """

    HEADER_SIZE = 8  # tag + length

    def __init__(self, data: bytes, path: str | None = None) -> None:
        if not data:
            raise IffError("empty IFF data")
        self._data = data
        self._path = path or "<memory>"
        self._stack: list[_StackFrame] = [_StackFrame(start=0, length=len(data))]
        self._chunk_read_pos: int = 0

    @classmethod
    def from_file(cls, path: str | Path) -> "IffReader":
        p = Path(path)
        return cls(p.read_bytes(), str(p))

    @property
    def path(self) -> str:
        return self._path

    @property
    def data(self) -> bytes:
        return self._data

    # --- navigation ---

    def at_end_of_form(self) -> bool:
        frame = self._stack[-1]
        if frame.in_chunk:
            return False
        return frame.used >= frame.length

    def current_name(self) -> int:
        """Tag of current block (form type if inside form after enter_form)."""
        block = self._peek_block()
        if block.is_form:
            if block.form_type is not None:
                return block.form_type
            return block.tag
        return block.tag

    def _peek_block(self) -> IffBlock:
        frame = self._stack[-1]
        if frame.in_chunk:
            raise IffError("inside chunk — cannot peek block")
        if self.at_end_of_form():
            raise IffError("at end of form")
        pos = frame.start + frame.used
        if pos + self.HEADER_SIZE > len(self._data):
            raise IffError(f"truncated block header at {pos:#x}")
        tag = struct.unpack_from(">I", self._data, pos)[0]
        length = struct.unpack_from(">I", self._data, pos + 4)[0]
        is_form = tag == TAG_FORM
        data_offset = pos + self.HEADER_SIZE
        form_type = None
        if is_form:
            if length < 4:
                raise IffError(f"FORM too short at {pos:#x}")
            form_type = struct.unpack_from(">I", self._data, data_offset)[0]
            data_offset += 4
        return IffBlock(
            tag=tag,
            length=length,
            offset=pos,
            data_offset=data_offset,
            is_form=is_form,
            form_type=form_type,
        )

    def enter_form(self, expected: int | None = None) -> None:
        block = self._peek_block()
        if not block.is_form:
            raise IffError(f"expected FORM at {block.offset:#x}, got {block.tag_str!r}")
        if expected is not None and block.form_type != expected:
            got = tag_to_str(block.form_type or 0)
            want = tag_to_str(expected)
            raise IffError(f"expected form {want!r}, got {got!r} at {block.offset:#x}")
        payload_len = block.length - 4  # form type tag included in length
        child_start = block.data_offset
        self._stack[-1].used += self.HEADER_SIZE + block.length
        self._stack.append(_StackFrame(start=child_start, length=payload_len))
        self._chunk_read_pos = 0

    def exit_form(self, expected: int | None = None) -> None:
        if len(self._stack) <= 1:
            raise IffError("cannot exit root form")
        if self._stack[-1].in_chunk:
            raise IffError("still inside chunk")
        if expected is not None:
            # parent block was already consumed; check current frame context
            pass
        self._stack.pop()

    def enter_chunk(self, expected: int | None = None) -> None:
        block = self._peek_block()
        if block.is_form:
            raise IffError(f"expected chunk at {block.offset:#x}, got FORM")
        if expected is not None and block.tag != expected:
            raise IffError(
                f"expected chunk {tag_to_str(expected)!r}, "
                f"got {block.tag_str!r} at {block.offset:#x}"
            )
        self._stack[-1].used += self.HEADER_SIZE + block.length
        self._stack.append(
            _StackFrame(start=block.data_offset, length=block.length, in_chunk=True)
        )
        self._chunk_read_pos = 0

    def exit_chunk(self, expected: int | None = None) -> None:
        if not self._stack[-1].in_chunk:
            raise IffError("not inside chunk")
        self._stack.pop()

    def iter_blocks(self) -> Iterator[IffBlock]:
        while not self.at_end_of_form():
            yield self._peek_block()

    def skip_block(self) -> None:
        block = self._peek_block()
        self._stack[-1].used += self.HEADER_SIZE + block.length

    def read_raw_block(self) -> bytes:
        """Return serialized bytes for the next block (header + payload) and advance."""
        block = self._peek_block()
        total = self.HEADER_SIZE + block.length
        raw = self._data[block.offset : block.offset + total]
        self.skip_block()
        return raw

    # --- chunk reads (little-endian) ---

    def _read_raw(self, size: int) -> bytes:
        frame = self._stack[-1]
        if not frame.in_chunk:
            raise IffError("not inside chunk")
        if self._chunk_read_pos + size > frame.length:
            raise IffError(
                f"chunk read overflow: need {size} bytes, "
                f"{frame.length - self._chunk_read_pos} left "
                f"in {self._path}"
            )
        off = frame.start + self._chunk_read_pos
        buf = self._data[off : off + size]
        self._chunk_read_pos += size
        return buf

    def read_bool8(self) -> bool:
        return self.read_uint8() != 0

    def read_uint8(self) -> int:
        return self._read_raw(1)[0]

    def read_int8(self) -> int:
        return struct.unpack("<b", self._read_raw(1))[0]

    def read_int16(self) -> int:
        return struct.unpack("<h", self._read_raw(2))[0]

    def read_uint16(self) -> int:
        return struct.unpack("<H", self._read_raw(2))[0]

    def read_int32(self) -> int:
        return struct.unpack("<i", self._read_raw(4))[0]

    def read_uint32(self) -> int:
        return struct.unpack("<I", self._read_raw(4))[0]

    def read_float(self) -> float:
        return struct.unpack("<f", self._read_raw(4))[0]

    def read_bytes(self, count: int) -> bytes:
        return self._read_raw(count)

    def read_string(self) -> str:
        """Null-terminated string (SOE read_string single value)."""
        chars: list[str] = []
        while True:
            b = self.read_uint8()
            if b == 0:
                break
            chars.append(chr(b))
        return "".join(chars)

    def read_float_vector(self) -> tuple[float, float, float]:
        return (self.read_float(), self.read_float(), self.read_float())

    def chunk_bytes_remaining(self) -> int:
        frame = self._stack[-1]
        if not frame.in_chunk:
            raise IffError("not inside chunk")
        return frame.length - self._chunk_read_pos

    def read_chunk_remaining(self) -> bytes:
        return self._read_raw(self.chunk_bytes_remaining())


def validate_iff(data: bytes) -> bool:
    """Port of IffNamespace::isValid — structural check only."""

    def walk(pos: int, end: int) -> bool:
        while pos < end:
            if pos + 8 > end:
                return False
            block_tag = struct.unpack_from(">I", data, pos)[0]
            block_length = struct.unpack_from(">I", data, pos + 4)[0]
            if block_length < 0 or pos + 8 + block_length > end:
                return False
            payload = pos + 8
            if block_tag == TAG_FORM:
                sub = block_length - 4
                if sub > 0 and not walk(payload + 4, payload + 4 + sub):
                    return False
            pos = payload + block_length
        return True

    return walk(0, len(data))


def root_form_type(data: bytes) -> int | None:
    """Return type tag of outermost FORM, or None."""
    if len(data) < 12:
        return None
    if struct.unpack_from(">I", data, 0)[0] != TAG_FORM:
        return None
    return struct.unpack_from(">I", data, 8)[0]
