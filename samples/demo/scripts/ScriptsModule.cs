using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Zypher.Scripting;

namespace Zypher.Demo.Scripts;

/// <summary>
/// Entry point exported from the game assembly. The engine loads this library
/// and calls <c>zypher_script_module()</c> to obtain the behaviour registry.
/// </summary>
public static class ScriptsModule
{
    [UnmanagedCallersOnly(EntryPoint = "zypher_script_module")]
    public static IntPtr ZypherScriptModule()
    {
        var registry = new ScriptRegistry();
        registry.Add<PlayerController>("PlayerController");
        registry.Add<Spinner>("Spinner");
        return ScriptHostBridge.CreateModule("Zypher Demo Scripts", registry);
    }
}
