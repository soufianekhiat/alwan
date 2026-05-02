using System;
using System.IO;
using Sharpmake;

namespace Alwan
{
    // Custom target
    [Generate]
    public class AlwanTarget : Target
    {
        public AlwanTarget()
            : base()
        {
        }

        public AlwanTarget(Platform platform, DevEnv devEnv, Optimization optimization)
            : base(platform, devEnv, optimization)
        {
        }
    }

    // Common project base for all Alwan projects
    public abstract class CommonProject : Project
    {
        public string WorkingDir = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "working_dir");

        public CommonProject() : base(typeof(AlwanTarget))
        {
            IsFileNameToLower = false;
            IsTargetFileNameToLower = false;
            SourceRootPath = @"[project.SharpmakeCsPath]\..\..\..\src";

            AddTargets(new AlwanTarget(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
            ));
        }

        [Configure()]
        public virtual void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            conf.Name = "[target.Optimization]";

            conf.ProjectFileName = "[project.Name]_[target.DevEnv]_[target.Platform]";
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "projects", "[project.Name]");

            conf.IntermediatePath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "tmp", "[project.Name]", conf.Name);
            conf.TargetPath = WorkingDir;
            conf.TargetLibraryPath = Path.Combine("[project.SharpmakeCsPath]", "..", "..", "..", "tmp", "lib", "win64_" + conf.Name);

            // C11 standard (use compiler option directly)
            conf.AdditionalCompilerOptions.Add("/std:c11");

            // Warning level 4, treat warnings as errors
            conf.AdditionalCompilerOptions.Add("/W4");
            conf.AdditionalCompilerOptions.Add("/WX");

            // Compile as C code (not C++)
            conf.AdditionalCompilerOptions.Add("/TC");

            // Precise floating point for accurate color science calculations
            conf.AdditionalCompilerOptions.Add("/fp:precise");

            // Use 64-bit host cl.exe. The 32-bit host (default on some VS installs)
            // hits C1060 "out of heap space" on large embedded-data TUs such as
            // alwan_spectrum_upsample.c (Jakob2019 LUTs, ~470K float literals/TU).
            // /Zm400 is kept as defense in depth for any fallback to 32-bit cl.
            conf.Options.Add(Options.Vc.General.PreferredToolArchitecture.x64);
            conf.AdditionalCompilerOptions.Add("/Zm400");

            // Basic defines
            conf.Defines.Add("NOMINMAX");
            conf.Defines.Add("WIN32");
            conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

            // Data embedding (default: embed)
            conf.Defines.Add("ALWAN_EMBED_DATA=1");

            // Disable range normalization for tests (test references use native mathematical ranges).
            // Library header defaults to ALWAN_NORMALIZE_RANGES=1 for end users.
            conf.Defines.Add("ALWAN_NORMALIZE_RANGES=0");

            // Optimization settings
            if (target.Optimization == Optimization.Debug)
            {
                conf.Defines.Add("_DEBUG");
                conf.Options.Add(Options.Vc.Compiler.Optimization.Disable);
                conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreadedDebug);
            }
            else
            {
                conf.Defines.Add("NDEBUG");
                conf.Options.Add(Options.Vc.Compiler.Optimization.MaximizeSpeed);
                conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreaded);
                conf.Options.Add(Options.Vc.Compiler.Inline.AnySuitable);

                // Enable AVX2 for wider SIMD (f32 x8, f64 x4)
                conf.AdditionalCompilerOptions.Add("/arch:AVX2");
            }

            // Set working directory for Visual Studio debugging
            conf.VcxprojUserFile = new Configuration.VcxprojUserFileSettings();
            conf.VcxprojUserFile.LocalDebuggerWorkingDirectory = "$(SolutionDir)working_dir";
        }
    }
}
