# MindControl — private Google Play internal testing

This guide applies to the Unreal Engine 5.7 Android project in `unreal/`.
It does **not** publish the game or certify the Ocean Serenity character.

## Play Console

1. Open [Google Play Console](https://play.google.com/console).
2. Create an app named **MindControl**, choose **Game**, and complete the required declarations.
3. Keep the existing Android application ID `com.eightytwogtstang.mindcontrol`. Once used in Play, it cannot be changed for that listing.
4. In **Testing > Internal testing**, create a release and add only the intended Google account(s) as testers. Use the opt-in link with the same account on the Android phone.
5. Complete the app content, data safety, and other required Play Console forms. Internal testing is not a public production launch, but Google may still review the submission.
6. Upload a **signed Android App Bundle (.aab)**. An APK from the existing development workflow is not a substitute for an AAB.

## Release-signing prerequisites

- Use a dedicated **upload key** (keystore), stored privately and backed up securely.
- Enroll in **Play App Signing** as directed by Play Console. The upload key and Play's app-signing key are distinct.
- Never commit a `.jks`, `.keystore`, passwords, or private signing configuration to Git.
- When automating builds, provide the keystore and passwords through GitHub Actions secrets or a securely configured self-hosted runner; mask secret values in logs.
- Configure Unreal's **Project Settings > Platforms > Android > Distribution Signing** with the keystore filename, alias, and passwords, and place the keystore in the project Android build directory on the trusted runner only.
- Configure Unreal Android distribution/AAB output for the installed Unreal version, then run a separate release build. Keep `.github/workflows/unreal-android.yml` intact for development APK testing.
- Increment `versionCode` for every Play upload. A release must be signed consistently with the registered upload key.
- Verify the resulting AAB with Android `bundletool` or Play Console, then install the Play-distributed test build on a device before claiming it is working.

## Status / quality gate

The existing workflow builds a **development APK** and imports the legacy experimental Ocean Serenity rig. A successful build does not prove that the approved photorealistic heroine is imported at `/Game/Characters/OceanSerenityHeroine/OceanSerenityHeroine`, that all animations work, or that the ring-finger ridge is smoothed. Those are separate requirements to validate on the actual packaged game.

**Next prerequisite:** Set up the Play Console app listing and upload signing key before adding a release workflow. Do not add placeholder secrets or modify the existing working development workflow.
