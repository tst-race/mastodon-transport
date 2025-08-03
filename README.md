# Mastodon Transport Component Plugin

Plugin providing a Mastodon transport component that works by posting and/or polling a specified hashtag. `source/manifest.json` shows two demonstration compositions using the `rapidUser` or `twoSixFileBasedUserModel` and the `base64` encoding components from the [decomposed exemplars](https://github.com/tst-race/decomposed-exemplars) plugin and the `SRICLIEncoding` encoding components from [Destini](https://github.com/tst-race/destini) plugin.

Whether text or images are posted depends on the JSON of the action provided by the user model. For example, this action will trigger posting a text status:

```
{"linkId": "*", "type": "post", "contentType": "text"}
```

This action triggers posting an image status:
```
{"linkId": "*", "type": "post", "contentType": "image"}
```

The polling side does not differentiate, and will pull data based on a "fetch" action and automatically attempt to use a text or image encoding to decode basedd on the content type that is fetched.


Example fetch action:
```
{"linkId": "*", "type": "fetch"}
```

## Required Parameters

Using the mastodon transport requires specifying the mastodon server and an accessToken for using the mastodon APIs, example flags for `race-cli` execution:

```
       --param mastodonCompositionFile.mastodonServer="https://mastodon.social" 
       --param mastodonCompositionFile.accessToken="dakjwKz2HsMJjjZ651akjEMAx5cKeU3sMzXHSD7ZVnw" 
```

The transport expects addresses specified with a `hashtag`, `maxTries`, and `timestamp` parameters:

```
       --recv-address="{\"hashtag\":\"jjkjjjj4\",\"maxTries\":120,\"timestamp\":0.0}"
```

maxTries and timestamp parameters are not currently used.

## Warnings

This transport, as-specified, is clearly not secure. In particular, the dynamically generated hashtags are not designed to blend in, and the use of base64 encoded text on a human-centered content service is obviously strange.
