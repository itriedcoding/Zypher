using System.Runtime.CompilerServices;

// The test harness drives behaviours with a fake native context, so it needs
// the internal ABI wrappers (ScriptContext, EntityHandle, NativeApi).
[assembly: InternalsVisibleTo("Zypher.Scripting.Tests")]
