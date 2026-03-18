using Sharpmake;

[module: Sharpmake.Include("common.cs")]
[module: Sharpmake.Include("AlwanLib.cs")]
[module: Sharpmake.Include("AlwanTests.cs")]
[module: Sharpmake.Include("AlwanBench.cs")]

namespace Alwan
{
    [Generate]
    public class AlwanSolution : Solution
    {
        public AlwanSolution() : base(typeof(AlwanTarget))
        {
            Name = "Alwan";
            IsFileNameToLower = false;

            AddTargets(new AlwanTarget(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
            ));
        }

        [Configure]
        public void ConfigureAll(Configuration conf, AlwanTarget target)
        {
            conf.Name = "[target.Optimization]";

            conf.SolutionFileName = "[solution.Name]_[target.DevEnv]_[target.Platform]";
            conf.SolutionPath = @"[solution.SharpmakeCsPath]\..\..\..";

            // Add library project
            conf.AddProject<AlwanLibProject>(target);

            // Add unified test project
            conf.AddProject<AlwanTestsProject>(target);

            // Add benchmark project
            conf.AddProject<AlwanBenchProject>(target);
        }

        [Main]
        public static void SharpmakeMain(Arguments args)
        {
            args.Generate<AlwanSolution>();
        }
    }
}
