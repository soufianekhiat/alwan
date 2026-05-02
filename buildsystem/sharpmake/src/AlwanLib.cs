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

            // Output type: Static library
            conf.Output = Configuration.OutputType.Lib;

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\src\alwan");

            // Source files (explicitly list them for now)
            conf.SourceFilesBuildExcludeRegex.Add(@".*\.sharpmake\.cs$");
            conf.SourceFilesBuildExcludeRegex.Add(@".*\.inc$");

            // Post-build: verify core .h vs .inc parity. The script reads each
            // alwan_*_core.h / alwan_*_core.inc pair, normalises macro layers,
            // and exits non-zero on any drift between function bodies. A failure
            // here means the C-backend dual-precision template (.inc) and the
            // GPU/single-backend mirror (.h) have diverged — usually because a
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
