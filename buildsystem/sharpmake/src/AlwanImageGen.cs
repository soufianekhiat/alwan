using System.IO;
using Sharpmake;

namespace Alwan
{
    [Generate]
    public class AlwanImageGenProject : CommonProject
    {
        public AlwanImageGenProject()
        {
            Name = "AlwanImageGen";
            SourceRootPath = @"[project.SharpmakeCsPath]\..\..\..\image_gen\";

            SourceFilesExtensions.Add(".c");
            SourceFilesExtensions.Add(".cpp");

            // Exclude build/ and output/
            SourceFilesExcludeRegex.Add(@"build[/\\]");
            SourceFilesExcludeRegex.Add(@"output[/\\]");

            // Exclude extern/ headers (compiled via #include from .cpp).
            // Keep extern/miniz.c — it's the only compilable unit tinyexr needs.
            SourceFilesExcludeRegex.Add(@"extern[/\\].*\.h$");
            SourceFilesExcludeRegex.Add(@"extern[/\\].*\.hh$");
            SourceFilesExcludeRegex.Add(@"miniz_impl\.c$");
        }

        [Configure()]
        public override void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            base.ConfigureAll(conf, target);

            conf.Output = Configuration.OutputType.Exe;

            // Remove /TC from parent — this project has mixed C/C++ (.cpp for tinyexr).
            // MSVC auto-detects language from extension: .c = C, .cpp = C++.
            conf.AdditionalCompilerOptions.Remove("/TC");

            // tinyexr uses C++ STL which requires exception handling
            conf.AdditionalCompilerOptions.Add("/EHsc");

            // Include paths
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\src\alwan");
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\image_gen");
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\image_gen\extern");

            // stb headers (sibling repo)
            conf.IncludePaths.Add(@"[project.SharpmakeCsPath]\..\..\..\..\stb");

            // Link against Alwan library
            conf.AddPrivateDependency<AlwanLibProject>(target);
        }
    }
}
