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
            SourceRootPath = @"[project.SharpmakeCsPath]\..\src\alwan";
        }

        [Configure()]
        public override void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            base.ConfigureAll(conf, target);

            // Output type: Static library
            conf.Output = Configuration.OutputType.Lib;

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\src\alwan");

            // Source files (explicitly list them for now)
            conf.SourceFilesBuildExcludeRegex.Add(@".*\.sharpmake\.cs$");
        }
    }
}
