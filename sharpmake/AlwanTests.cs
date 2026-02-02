using System.IO;
using Sharpmake;

namespace Alwan
{
    // Unified test project that runs all tests consecutively
    [Generate]
    public class AlwanTestsProject : CommonProject
    {
        public AlwanTestsProject()
        {
            Name = "AlwanTests";
            SourceRootPath = @"[project.SharpmakeCsPath]\..\tests\";

            // Include all test files plus the test runner
            SourceFilesExtensions.Add(".c");
        }

        [Configure()]
        public override void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            base.ConfigureAll(conf, target);

            // Output type: Console executable
            conf.Output = Configuration.OutputType.Exe;

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\src\alwan");

            // Link against Alwan library
            conf.AddPrivateDependency<AlwanLibProject>(target);
        }
    }
}
