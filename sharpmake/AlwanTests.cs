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
            /*
			SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\test_runner.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\00_context.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\01_mat3_ops.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\02_data_embed_compile.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\03_rgb_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\04_srgb_tf.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\05_xyz_lab_luv.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\06_delta_e.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\07_cat_matrices.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\08_cat_roundtrip.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\09_tf_hdr.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\10_view_transforms.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\11_spd_to_xyz.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\12_bandpass_2012.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\13_ciecam02.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\14_cam16.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\15_conv_convenience.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\16_extended_colorspaces.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\17_quality_cct.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\18_gamut.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\19_rgb_convert.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\20_oklab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\21_ictcp.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\22_jzazbz.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\23_din99.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\24_osa_ucs.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\25_hunter_lab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\26_ipt.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\27_prolab.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\28_zcam.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\29_hunt.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\30_delta_e_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\31_whiteness_yellowness.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\32_quality_rendering.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\33_rgb_spaces_p5.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\34_tf_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\35_cat_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\36_spectral_extended.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\37_rgb_to_spectrum.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\38_camera_sensitivities.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\39_spd_shape.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\40_gamut_analysis.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\41_vision_perception.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\42_math_utilities.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\43_reference_data.c");
            SourceFiles.Add(@"[project.SharpmakeCsPath]\..\tests\44_color_correction.c");*/
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
