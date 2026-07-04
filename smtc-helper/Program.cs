using System.Text.Json;
using Windows.Media.Control;

var jsonOptions = new JsonSerializerOptions
{
    PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
};

// Output ready event on startup
WriteJson(new { type = "ready", version = "1.0" });

try
{
    var manager = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();

    // Fire initial state if a session is already active
    if (manager.GetCurrentSession() is { } initialSession)
    {
        await EmitMediaState(initialSession);
    }

    // Subscribe to session changes
    manager.CurrentSessionChanged += async (sender, args) =>
    {
        try
        {
            var session = sender.GetCurrentSession();
            if (session != null)
            {
                await EmitMediaState(session);
            }
            else
            {
                // No active session — we could emit a "stopped" event, but
                // the spec only says media events. Silently ignore for now.
            }
        }
        catch (Exception ex)
        {
            WriteJson(new { type = "error", message = $"CurrentSessionChanged error: {Sanitize(ex.Message)}", fatal = false });
        }
    };

    // Listen indefinitely
    await Task.Delay(Timeout.Infinite);
}
catch (Exception ex)
{
    WriteJson(new { type = "error", message = $"Fatal error: {Sanitize(ex.Message)}", fatal = true });
    Environment.Exit(1);
}

async Task EmitMediaState(GlobalSystemMediaTransportControlsSession session)
{
    try
    {
        // Subscribe to session events on first encounter
        // Use a concurrent-safe flag so we only attach once per session
        var sessionId = session.SourceAppName ?? session.GetHashCode().ToString();
        // We rely on the fact that the session object is stable; attach handlers once.
        // Use a static HashSet to track which sessions we've wired up.
        lock (_subscribedSessionsLock)
        {
            if (_subscribedSessions.Add(sessionId))
            {
                session.MediaPropertiesChanged += OnMediaPropertiesChanged;
                session.PlaybackInfoChanged += OnPlaybackInfoChanged;
                session.TimelinePropertiesChanged += OnTimelinePropertiesChanged;
            }
        }

        await OutputCurrentState(session);
    }
    catch (Exception ex)
    {
        WriteJson(new { type = "error", message = $"EmitMediaState error: {Sanitize(ex.Message)}", fatal = false });
    }
}

async void OnMediaPropertiesChanged(GlobalSystemMediaTransportControlsSession session, MediaPropertiesChangedEventArgs args)
{
    try
    {
        await OutputCurrentState(session);
    }
    catch (Exception ex)
    {
        WriteJson(new { type = "error", message = $"MediaPropertiesChanged error: {Sanitize(ex.Message)}", fatal = false });
    }
}

async void OnPlaybackInfoChanged(GlobalSystemMediaTransportControlsSession session, PlaybackInfoChangedEventArgs args)
{
    try
    {
        await OutputCurrentState(session);
    }
    catch (Exception ex)
    {
        WriteJson(new { type = "error", message = $"PlaybackInfoChanged error: {Sanitize(ex.Message)}", fatal = false });
    }
}

async void OnTimelinePropertiesChanged(GlobalSystemMediaTransportControlsSession session, TimelinePropertiesChangedEventArgs args)
{
    try
    {
        await OutputCurrentState(session);
    }
    catch (Exception ex)
    {
        WriteJson(new { type = "error", message = $"TimelinePropertiesChanged error: {Sanitize(ex.Message)}", fatal = false });
    }
}

async Task OutputCurrentState(GlobalSystemMediaTransportControlsSession session)
{
    try
    {
        var mediaProps = await session.TryGetMediaPropertiesAsync();
        var timeline = session.GetTimelineProperties();
        var playbackInfo = session.GetPlaybackInfo();

        var status = playbackInfo.PlaybackStatus switch
        {
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing => "playing",
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Paused => "paused",
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Stopped => "stopped",
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Closed => "closed",
            GlobalSystemMediaTransportControlsSessionPlaybackStatus.Changing => "changing",
            _ => "unknown"
        };

        var json = new
        {
            type = "media",
            title = mediaProps.Title ?? "",
            artist = mediaProps.Artist ?? "",
            album = mediaProps.AlbumTitle ?? "",
            position = timeline.Position.TotalSeconds,
            duration = timeline.EndTime.TotalSeconds,
            status,
            sourceApp = session.SourceAppName ?? ""
        };

        WriteJson(json);
    }
    catch (Exception ex)
    {
        WriteJson(new { type = "error", message = $"OutputCurrentState error: {Sanitize(ex.Message)}", fatal = false });
    }
}

void WriteJson(object obj)
{
    var json = JsonSerializer.Serialize(obj, jsonOptions);
    Console.WriteLine(json);
    Console.Out.Flush();
}

// Minimal sanitization: replace control characters to prevent JSON corruption
string Sanitize(string message)
{
    return message?.Replace("\"", "'").Replace("\r", " ").Replace("\n", " ") ?? "";
}

// Thread-safe set for tracking subscribed sessions
static readonly HashSet<string> _subscribedSessions = new();
static readonly object _subscribedSessionsLock = new();
