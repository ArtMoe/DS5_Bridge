using NAudio.CoreAudioApi;

sealed class EndpointManager
{
    private static readonly string[] BridgeEndpointAliases =
    [
        "DS5 Bridge",
        "DualSense Wireless Controller"
    ];

    public static MMDevice SelectRenderEndpoint(MMDeviceEnumerator enumerator, string? deviceName)
    {
        var devices = enumerator
            .EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active)
            .ToArray();

        if (!string.IsNullOrWhiteSpace(deviceName))
        {
            return SelectNamedEndpoint(devices, deviceName, "Render");
        }

        var bridge = FindKnownBridgeEndpoint(devices);
        if (bridge is not null)
        {
            return bridge;
        }

        return enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
    }

    public static MMDevice SelectCaptureEndpoint(MMDeviceEnumerator enumerator, string? deviceName)
    {
        var devices = enumerator
            .EnumerateAudioEndPoints(DataFlow.Capture, DeviceState.Active)
            .ToArray();

        if (!string.IsNullOrWhiteSpace(deviceName))
        {
            return SelectNamedEndpoint(devices, deviceName, "Capture");
        }

        var bridge = FindKnownBridgeEndpoint(devices);
        if (bridge is not null)
        {
            return bridge;
        }

        return enumerator.GetDefaultAudioEndpoint(DataFlow.Capture, Role.Communications);
    }

    public static MMDevice? FindKnownBridgeEndpoint(IEnumerable<MMDevice> devices)
    {
        foreach (var name in BridgeEndpointAliases)
        {
            var match = devices.FirstOrDefault(device =>
                device.FriendlyName.Contains(name, StringComparison.OrdinalIgnoreCase));
            if (match is not null)
            {
                return match;
            }
        }

        return null;
    }

    public static void ListDevices()
    {
        using var enumerator = new MMDeviceEnumerator();
        foreach (var device in enumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active))
        {
            Console.Error.WriteLine($"render-device: {device.FriendlyName}");
        }
        foreach (var device in enumerator.EnumerateAudioEndPoints(DataFlow.Capture, DeviceState.Active))
        {
            Console.Error.WriteLine($"capture-device: {device.FriendlyName}");
        }
    }

    private static MMDevice SelectNamedEndpoint(MMDevice[] devices, string deviceName, string role)
    {
        var exact = devices.FirstOrDefault(device =>
            string.Equals(device.FriendlyName, deviceName, StringComparison.OrdinalIgnoreCase));
        if (exact is not null)
        {
            return exact;
        }

        var contains = devices.FirstOrDefault(device =>
            device.FriendlyName.Contains(deviceName, StringComparison.OrdinalIgnoreCase));
        if (contains is not null)
        {
            return contains;
        }

        if (deviceName.Contains("DS5 Bridge", StringComparison.OrdinalIgnoreCase))
        {
            var alias = FindKnownBridgeEndpoint(devices);
            if (alias is not null)
            {
                if (AudioConstants.DiagnosticsEnabled)
                {
                    Console.Error.WriteLine($"status: endpoint alias '{alias.FriendlyName}' matched for '{deviceName}'");
                }
                return alias;
            }
        }

        var available = string.Join(", ", devices.Select(device => $"'{device.FriendlyName}'"));
        throw new InvalidOperationException($"{role} endpoint matching '{deviceName}' was not found. Available endpoints: {available}");
    }
}
