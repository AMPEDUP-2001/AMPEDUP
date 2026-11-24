"""LLDB command for the AMPEDUP! v0.7.3 European-disc Bink callback."""
import lldb


CALLBACK = 0x00242C4C


def watch(debugger, command, exe_ctx, result, internal_dict):
    """Install a four-byte hardware write watchpoint after Xbox memory is mapped."""
    if command.strip():
        result.SetError("Usage: amped-bink-watch")
        return
    target = exe_ctx.GetTarget()
    process = exe_ctx.GetProcess()
    if not process.IsValid() or process.GetState() != lldb.eStateStopped:
        result.SetError("Stop the native game at xbox_kernel_init after memory mapping.")
        return
    offset = target.FindFirstGlobalVariable("g_xbox_mem_offset")
    if not offset.IsValid():
        result.SetError("Load symbols for g_xbox_mem_offset from the matching unstripped build.")
        return
    error = lldb.SBError()
    base = offset.GetValueAsUnsigned(error)
    if error.Fail():
        result.SetError("Could not read the Xbox mapping offset: " + str(error))
        return
    address = (base + CALLBACK) % (1 << (target.GetAddressByteSize() * 8))
    initial = process.ReadUnsignedFromMemory(address, 4, error)
    if error.Fail():
        result.SetError("The callback is unreadable. Check that Xbox memory mapping has finished.")
        return
    point = target.WatchAddress(address, 4, False, True, error)
    if error.Fail() or not point.IsValid():
        result.SetError("Could not arm a four-byte hardware watchpoint: " + str(error))
        return
    result.AppendMessage(f"Bink guest 0x{CALLBACK:08x} initial=0x{initial:08x}, "
                         f"watchpoint {point.GetID()}. Use continue, then bt at each stop.")


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand("command script add -f " + __name__ + ".watch amped-bink-watch")
