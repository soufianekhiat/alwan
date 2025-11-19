using System.IO;
using Sharpmake;

namespace Alwan
{
    [Generate]
    public class RGB2SpecToolProject : Project
    {
        public RGB2SpecToolProject() : base(typeof(Target))
        {
            Name = "RGB2SpecTool";
            IsFileNameToLower = false;
            IsTargetFileNameToLower = false;
            SourceRootPath = @"[project.SharpmakeCsPath]\..\tools\rgb2spec";

            // Add source file at project level
            SourceFiles.Add(@"[project.SourceRootPath]\rgb2spec_opt.cpp");

            // Simple single-target configuration (Release x64 only for tool)
            AddTargets(new Target(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Release
            ));
        }

        [Configure()]
        public void ConfigureAll(Configuration conf, Target target)
        {
            conf.Name = "[target.Optimization]";
            conf.ProjectFileName = "[project.Name]_[target.DevEnv]_[target.Platform]";
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "..", "projects", "[project.Name]");

            conf.IntermediatePath = Path.Combine("[project.SharpmakeCsPath]", "..", "tmp", "[project.Name]", conf.Name);
            conf.TargetPath = Path.Combine("[project.SharpmakeCsPath]", "..", "tools", "datagen");
            conf.TargetFileName = "gen_jakob2019_table";

            // Output type: Console application
            conf.Output = Configuration.OutputType.Exe;

            // Include paths
            conf.IncludePaths.Add(@"[project.SourceRootPath]");
            conf.IncludePaths.Add(@"[project.SourceRootPath]\details");

            // Compile as C++ (not C)
            // No need for /TC flag - it's C++ by default

            // C++17 standard
            conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);

            // Warning level 4
            conf.AdditionalCompilerOptions.Add("/W4");

            // Basic defines
            conf.Defines.Add("WIN32");
            conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

            // Disable OpenMP - MSVC only supports OpenMP 2.0 which lacks 'collapse' clause
            // The code will still run fast without OpenMP (much faster than Python)

            // Optimization settings
            conf.Defines.Add("NDEBUG");
            conf.Options.Add(Options.Vc.Compiler.Optimization.MaximizeSpeed);
            conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreaded);
            conf.Options.Add(Options.Vc.Compiler.Inline.AnySuitable);
            conf.Options.Add(Options.Vc.Compiler.FavorSizeOrSpeed.FastCode);

            // Enable intrinsics and fast floating point
            conf.Options.Add(Options.Vc.Compiler.FloatingPointModel.Fast);
        }
    }
}
