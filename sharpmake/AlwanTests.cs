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
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\03_rgb_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\04_srgb_tf.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\05_xyz_lab_luv.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\06_delta_e.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\07_cat_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\08_cat_roundtrip.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\09_tf_hdr.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\10_view_transforms.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\11_spd_to_xyz.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\12_bandpass_2012.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\13_ciecam02.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\14_cam16.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\15_conv_convenience.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\16_extended_colorspaces.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\17_quality_cct.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\18_gamut.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\19_rgb_convert.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\20_oklab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\21_ictcp.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\22_jzazbz.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\23_din99.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\24_osa_ucs.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\25_hunter_lab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\26_ipt.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\27_prolab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\28_zcam.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\29_hunt.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\30_delta_e_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\31_whiteness_yellowness.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\32_quality_rendering.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\33_rgb_spaces_p5.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\34_tf_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\35_cat_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\36_spectral_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\37_rgb_to_spectrum.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\38_camera_sensitivities.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\39_spd_shape.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\40_gamut_analysis.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\41_vision_perception.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\42_math_utilities.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\43_reference_data.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\unit\44_color_correction.c");
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
