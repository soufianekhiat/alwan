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
            SourceRootPath = @"[project.SharpmakeCsPath]\..\tests\unit";

            // Include all test files plus the test runner
            SourceFilesExtensions.Add(".c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\test_runner.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\00_context.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\01_mat3_ops.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\02_data_embed_compile.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\10_rgb_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\11_srgb_tf.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\20_xyz_lab_luv.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\21_delta_e.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\30_cat_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\31_cat_roundtrip.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\40_tf_hdr.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\41_view_transforms.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\50_spd_to_xyz.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\60_bandpass_2012.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\70_ciecam02.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\80_cam16.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\90_conv_convenience.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\100_quality_cct.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\110_gamut.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\111_rgb_convert.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\120_oklab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\130_ictcp.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\240_quality_rendering.c");
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
