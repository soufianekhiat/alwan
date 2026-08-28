using System.IO;
using Sharpmake;

namespace Alwan
{
    [Generate]
    public class AlwanLibProject : CommonProject
    {
        public AlwanLibProject()
        {
            Name = "Alwan";
            SourceRootPath = @"[project.SharpmakeCsPath]\..\..\..\src\alwan";

            // Include .inc files in the project for IDE visibility (not compiled)
            SourceFilesExtensions.Add(".inc");
        }

        [Configure()]
        public override void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            base.ConfigureAll(conf, target);

            // Output type follows the LinkType fragment: static .lib or shared .dll.
            bool dll = (target.LinkType == LinkType.Dll);
            conf.Output = dll ? Configuration.OutputType.Dll : Configuration.OutputType.Lib;

            if (dll)
            {
                // MSVC has no "export all symbols" switch. Mirror CMake's
                // WINDOWS_EXPORT_ALL_SYMBOLS exactly: dump the just-compiled
                // object symbols into a .def, then link with it. EventPreLink
                // runs after compile (objects exist) and before link, so the
                // export set matches the build precisely -- no phantom symbols
                // and nothing missed, regardless of the f32/f64/det defines.
                string genDef = @"[project.SharpmakeCsPath]\..\..\..\tools\gen_exports_def.py";
                string defOut = @"[project.SharpmakeCsPath]\..\..\alwan_exports.def";
                // Note the trailing '.' on $(IntDir): the macro ends with a
                // backslash, so a bare "$(IntDir)" would become ...dir\" -- the
                // \" escapes the quote and corrupts arg parsing. "$(IntDir)."
                // ends the quote with .\" which is safe.
                conf.EventPreLink.Add(
                    "python \"" + genDef + "\" \"$(IntDir).\" \"" + defOut + "\"");
                // ModuleDefinitionFile resolves relative to SourceRootPath (src/alwan).
                conf.ModuleDefinitionFile = @"..\..\buildsystem\alwan_exports.def";
            }

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\src\alwan");

            // Source files (explicitly list them for now)
            conf.SourceFilesBuildExcludeRegex.Add(@".*\.sharpmake\.cs$");
            conf.SourceFilesBuildExcludeRegex.Add(@".*\.inc$");

            // Post-build: verify core .h vs .inc parity. The script reads each
            // alwan_*_core.h / alwan_*_core.inc pair, normalises macro layers,
            // and exits non-zero on any drift between function bodies. A failure
            // here means the C-backend dual-precision template (.inc) and the
            // GPU/single-backend mirror (.h) have diverged, usually because a
            // fix was applied to one without the other (see feedback note
            // "Core .h and .inc parity" for the bug history).
            //
            // The check is platform-agnostic Python and runs in well under a
            // second; we run it on every config so a divergence in either Debug
            // or Release surfaces immediately.
            string parityScript = @"[project.SharpmakeCsPath]\..\..\..\tools\check_core_parity.py";
            conf.EventPostBuild.Add(
                "python \"" + parityScript + "\" --quiet"
            );
        }
    }
}
