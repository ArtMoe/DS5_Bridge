using Xunit;

public sealed class EndpointManagerTests
{
    [Theory]
    [InlineData("DS5 Bridge")]
    [InlineData("Speakers (DS5 Bridge)")]
    [InlineData("Headphones (DualSense Wireless Controller)")]
    [InlineData("Headset Earphone (Wireless Controller)")]
    [InlineData("Speakers (Xbox 360 Controller for Windows)")]
    public void IsKnownBridgeEndpointNameAcceptsBridgePersonaNames(string friendlyName)
    {
        Assert.True(EndpointManager.IsKnownBridgeEndpointName(friendlyName));
    }

    [Fact]
    public void IsKnownBridgeEndpointNameRejectsUnrelatedEndpoints()
    {
        Assert.False(EndpointManager.IsKnownBridgeEndpointName("Speakers (Realtek USB Audio)"));
    }
}
