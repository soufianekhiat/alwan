using System.IO;
using Sharpmake;

namespace Alwan
{
    [Generate]
    public class AlwanBenchProject : CommonProject
    {
        public AlwanBenchProject()
        {
            Name = "AlwanBench";
            SourceRootPath = @"[project.SharpmakeCsPath]\..\..\..\bench\";

            SourceFilesExtensions.Add(".c");
        }

        [Configure()]
        public override void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            base.ConfigureAll(conf, target);

            // Output type: Console executable
            conf.Output = Configuration.OutputType.Exe;

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\src\alwan");

            // Link against Alwan library
            conf.AddPrivateDependency<AlwanLibProject>(target);
        }
    }
}
