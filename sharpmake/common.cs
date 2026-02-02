using System;
using System.IO;
using Sharpmake;

namespace Alwan
{
    // Scalar type fragment for float/double toggle
    [Fragment, Flags]
    public enum ScalarType
    {
        Float  = 1 << 0,  // float (32-bit)
        Double = 1 << 1   // double (64-bit, default)
    }

    // Custom target with ScalarType
    [Generate]
    public class AlwanTarget : Target
    {
        public AlwanTarget()
            : base()
        {
        }

        public AlwanTarget(Platform platform, DevEnv devEnv, Optimization optimization, ScalarType scalarType)
            : base(platform, devEnv, optimization)
        {
            Scalar = scalarType;
        }

        public ScalarType Scalar;
    }

    // Common project base for all Alwan projects
    public abstract class CommonProject : Project
    {
        public string WorkingDir = Path.Combine("[project.SharpmakeCsPath]", "..", "working_dir");

        public CommonProject() : base(typeof(AlwanTarget))
        {
            IsFileNameToLower = false;
            IsTargetFileNameToLower = false;
            SourceRootPath = @"[project.SharpmakeCsPath]\..\src";

            // Add all target permutations
            AddTargets(new AlwanTarget(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release,
                ScalarType.Float | ScalarType.Double
            ));
        }

        [Configure()]
        public virtual void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            // Name includes scalar type: Debug_f32, Release_f64, etc.
            string scalarSuffix = (target.Scalar == ScalarType.Float) ? "f32" : "f64";
            conf.Name = "[target.Optimization]_" + scalarSuffix;

            conf.ProjectFileName = "[project.Name]_[target.DevEnv]_[target.Platform]";
            conf.ProjectPath = Path.Combine("[project.SharpmakeCsPath]", "..", "projects", "[project.Name]");

            conf.IntermediatePath = Path.Combine("[project.SharpmakeCsPath]", "..", "tmp", "[project.Name]", conf.Name);
            conf.TargetPath = WorkingDir;
            conf.TargetLibraryPath = Path.Combine("[project.SharpmakeCsPath]", "..", "tmp", "lib", "win64_" + conf.Name);

            // C11 standard (use compiler option directly)
            conf.AdditionalCompilerOptions.Add("/std:c11");

            // Warning level 4, treat warnings as errors
            conf.AdditionalCompilerOptions.Add("/W4");
            conf.AdditionalCompilerOptions.Add("/WX");

            // Compile as C code (not C++)
            conf.AdditionalCompilerOptions.Add("/TC");

            // Precise floating point for accurate color science calculations
            conf.AdditionalCompilerOptions.Add("/fp:precise");

            // Basic defines
            conf.Defines.Add("NOMINMAX");
            conf.Defines.Add("WIN32");
            conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

            // Add scalar type define
            if (target.Scalar == ScalarType.Float)
            {
                conf.Defines.Add("ALWAN_SCALAR_IS_FLOAT=1");
            }
            else
            {
                conf.Defines.Add("ALWAN_SCALAR_IS_FLOAT=0");
            }

            // Data embedding (default: embed)
            conf.Defines.Add("ALWAN_EMBED_DATA=1");

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
            }

            // Set working directory for Visual Studio debugging
            conf.VcxprojUserFile = new Configuration.VcxprojUserFileSettings();
            conf.VcxprojUserFile.LocalDebuggerWorkingDirectory = "$(SolutionDir)working_dir";
        }
    }
}
