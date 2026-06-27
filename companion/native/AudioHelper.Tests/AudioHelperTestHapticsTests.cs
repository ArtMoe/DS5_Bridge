using System.Buffers.Binary;
using NAudio.Wave;
using Xunit;

public sealed class AudioHelperTestHapticsTests
{
    [Fact]
    public void BuildTestPcmWritesOppositePhaseHapticsAfterSilentPreroll()
    {
        var format = new WaveFormat(48000, 16, 4);

        var pcm = AudioHelperTestHaptics.BuildTestPcm(format, 100);

        Assert.Equal(44 * 512 * format.BlockAlign, pcm.Length);
        Assert.Equal(0, ReadChannel(pcm, format, 0, 2));
        Assert.Equal(0, ReadChannel(pcm, format, 0, 3));

        var firstActiveFrame = 3 * 512;
        var secondActiveFrame = 4 * 512;
        Assert.True(ReadChannel(pcm, format, firstActiveFrame, 2) < 0);
        Assert.True(ReadChannel(pcm, format, firstActiveFrame, 3) > 0);
        Assert.True(ReadChannel(pcm, format, secondActiveFrame, 2) > 0);
        Assert.True(ReadChannel(pcm, format, secondActiveFrame, 3) < 0);
    }

    [Fact]
    public void BuildTestPcmClampsZeroGainToSilentHaptics()
    {
        var format = new WaveFormat(48000, 16, 4);

        var pcm = AudioHelperTestHaptics.BuildTestPcm(format, 0);

        Assert.All(pcm, sample => Assert.Equal(0, sample));
    }

    private static short ReadChannel(byte[] pcm, WaveFormat format, int frame, int channel)
    {
        var offset = frame * format.BlockAlign + channel * (format.BitsPerSample / 8);
        return BinaryPrimitives.ReadInt16LittleEndian(pcm.AsSpan(offset, 2));
    }
}
