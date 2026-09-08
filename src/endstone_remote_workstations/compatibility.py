"""Legacy static metadata gate. Only the loaded Windows bridge can grant native access."""
from .model import Rejected

LINUX_SHA256 = "8ba803f23d681816495c7ac83bdba4b9cd7165a3bee5aedd18fa0c8c3d408ec2"
WINDOWS_SHA256 = "92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f"


def require_native(binary_sha256, platform, runtime_version, protocol):
    if platform != "linux-x86_64" or binary_sha256 != LINUX_SHA256:
        raise Rejected("unknown native target; no BDS calls permitted")
    if runtime_version != "0.11.10" or protocol != 2169:
        raise Rejected("runtime/protocol is outside the inspected native target")
    raise Rejected("Linux native container ABI and Python bridge remain unverified; backend disabled")
