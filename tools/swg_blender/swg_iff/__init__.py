"""SWG IFF utilities."""

from swg_iff.reader import IffError, IffReader, root_form_type, validate_iff
from swg_iff.tags import KNOWN_ROOT_FORMS, tag_to_str

__all__ = [
    "IffError",
    "IffReader",
    "KNOWN_ROOT_FORMS",
    "root_form_type",
    "tag_to_str",
    "validate_iff",
]
