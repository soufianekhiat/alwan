using Sharpmake;

[module: Sharpmake.Include("common.cs")]
[module: Sharpmake.Include("AlwanLib.cs")]
[module: Sharpmake.Include("AlwanTests.cs")]
[module: Sharpmake.Include("RGB2SpecTool.cs")]

namespace Alwan
{
    [Generate]
    public class AlwanSolution : Solution
    {
        public AlwanSolution() : base(typeof(AlwanTarget))
        {
            Name = "Alwan";
            IsFileNameToLower = false;

            // Add targets for both float and double precision
            AddTargets(new AlwanTarget(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release,
                ScalarType.Float | ScalarType.Double
            ));
        }

        [Configure]
        public void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            // Make solution configuration names unique by including scalar type
            string scalarSuffix = (target.Scalar == ScalarType.Float) ? "_f32" : "_f64";
            conf.Name = "[target.Optimization]" + scalarSuffix;

            conf.SolutionFileName = "[solution.Name]_[target.DevEnv]_[target.Platform]";
            conf.SolutionPath = @"[solution.SharpmakeCsPath]\..\..\..";

            // Add library project
            conf.AddProject<AlwanLibProject>(target);

            // Add unified test project
            conf.AddProject<AlwanTestsProject>(target);

            // Add RGB2Spec data generation tool (Release only)
            if (target.Optimization == Optimization.Release)
            {
                // Use standard Target for the tool project
                var toolTarget = new Target(
                    target.Platform,
                    target.DevEnv,
                    Optimization.Release
                );
                conf.AddProject<RGB2SpecToolProject>(toolTarget);
            }
        }

        [Main]
        public static void SharpmakeMain(Arguments args)
        {
            args.Generate<AlwanSolution>();
        }
    }
}
