"use client";

import { useState, useEffect } from "react";

type Platform = "macos" | "linux" | "unknown";

interface ReleaseAsset {
  name: string;
  browser_download_url: string;
}

interface Release {
  tag_name: string;
  assets: ReleaseAsset[];
}

function detectPlatform(): Platform {
  if (typeof window === "undefined") return "unknown";

  const ua = navigator.userAgent.toLowerCase();
  if (ua.includes("mac")) return "macos";
  if (ua.includes("linux")) return "linux";
  return "unknown";
}

function getAssetForPlatform(
  assets: ReleaseAsset[],
  platform: Platform
): ReleaseAsset | undefined {
  if (platform === "macos") {
    return assets.find((a) => a.name.includes("arm64-osx"));
  }
  if (platform === "linux") {
    return assets.find((a) => a.name.includes("x64-linux"));
  }
  return undefined;
}

function getPlatformLabel(platform: Platform): string {
  switch (platform) {
    case "macos":
      return "macOS (Apple Silicon)";
    case "linux":
      return "Linux (x64)";
    default:
      return "Download";
  }
}

function getOtherPlatform(platform: Platform): Platform {
  return platform === "macos" ? "linux" : "macos";
}

export default function DownloadButton() {
  const [platform, setPlatform] = useState<Platform>("unknown");
  const [release, setRelease] = useState<Release | null>(null);
  const [loading, setLoading] = useState(true);
  const [showDropdown, setShowDropdown] = useState(false);

  useEffect(() => {
    setPlatform(detectPlatform());

    fetch("https://api.github.com/repos/PABannier/PathView/releases/latest")
      .then((res) => res.json())
      .then((data: Release) => {
        setRelease(data);
        setLoading(false);
      })
      .catch(() => {
        setLoading(false);
      });
  }, []);

  const primaryAsset = release
    ? getAssetForPlatform(release.assets, platform)
    : undefined;
  const otherPlatform = getOtherPlatform(platform);
  const otherAsset = release
    ? getAssetForPlatform(release.assets, otherPlatform)
    : undefined;

  // For unknown platforms, show both options
  const isUnsupported = platform === "unknown";

  if (loading) {
    return (
      <div className="flex flex-col items-center gap-3">
        <button
          disabled
          className="flex items-center gap-2 px-6 py-3 rounded-full bg-purple-600/50 text-white font-medium"
        >
          <svg className="w-5 h-5 animate-spin" viewBox="0 0 24 24">
            <circle
              className="opacity-25"
              cx="12"
              cy="12"
              r="10"
              stroke="currentColor"
              strokeWidth="4"
              fill="none"
            />
            <path
              className="opacity-75"
              fill="currentColor"
              d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
            />
          </svg>
          Loading...
        </button>
      </div>
    );
  }

  if (!release) {
    return (
      <a
        href="https://github.com/PABannier/PathView/releases"
        target="_blank"
        rel="noopener noreferrer"
        className="flex items-center gap-2 px-6 py-3 rounded-full bg-purple-600 hover:bg-purple-500 text-white font-medium transition-colors"
      >
        <DownloadIcon />
        View Releases on GitHub
      </a>
    );
  }

  if (isUnsupported) {
    return (
      <div className="flex flex-col items-center gap-4">
        <p className="text-amber-400 text-sm">
          PathView is currently available for macOS and Linux only.
        </p>
        <div className="flex flex-col sm:flex-row gap-3">
          {release.assets
            .filter(
              (a) =>
                a.name.includes("arm64-osx") || a.name.includes("x64-linux")
            )
            .map((asset) => (
              <a
                key={asset.name}
                href={asset.browser_download_url}
                className="flex items-center gap-2 px-5 py-2.5 rounded-full bg-gray-800 hover:bg-gray-700 text-white font-medium transition-colors border border-gray-700"
              >
                <DownloadIcon />
                {asset.name.includes("osx") ? "macOS" : "Linux"}
              </a>
            ))}
        </div>
        <p className="text-gray-500 text-xs">Version {release.tag_name}</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col items-center gap-3">
      <div className="relative">
        <div className="flex">
          {/* Primary download button */}
          <a
            href={primaryAsset?.browser_download_url || "#"}
            className="flex items-center gap-2 px-6 py-3 rounded-l-full bg-purple-600 hover:bg-purple-500 text-white font-medium transition-colors"
          >
            <DownloadIcon />
            Download for {getPlatformLabel(platform)}
          </a>

          {/* Dropdown toggle */}
          <button
            onClick={() => setShowDropdown(!showDropdown)}
            className="px-3 py-3 rounded-r-full bg-purple-600 hover:bg-purple-500 text-white border-l border-purple-500 transition-colors"
            aria-label="More download options"
          >
            <svg
              className={`w-4 h-4 transition-transform ${
                showDropdown ? "rotate-180" : ""
              }`}
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2}
                d="M19 9l-7 7-7-7"
              />
            </svg>
          </button>
        </div>

        {/* Dropdown menu */}
        {showDropdown && otherAsset && (
          <div className="absolute top-full left-0 right-0 mt-2 py-1 bg-gray-800 rounded-lg border border-gray-700 shadow-xl z-10">
            <a
              href={otherAsset.browser_download_url}
              className="flex items-center gap-2 px-4 py-2 text-gray-300 hover:bg-gray-700 hover:text-white transition-colors"
              onClick={() => setShowDropdown(false)}
            >
              <DownloadIcon />
              {getPlatformLabel(otherPlatform)}
            </a>
          </div>
        )}
      </div>

      <p className="text-gray-500 text-sm">
        Version {release.tag_name} &middot;{" "}
        <a
          href="https://github.com/PABannier/PathView/releases"
          target="_blank"
          rel="noopener noreferrer"
          className="text-gray-400 hover:text-white transition-colors"
        >
          View all releases
        </a>
      </p>
    </div>
  );
}

function DownloadIcon() {
  return (
    <svg
      className="w-5 h-5"
      fill="none"
      viewBox="0 0 24 24"
      stroke="currentColor"
    >
      <path
        strokeLinecap="round"
        strokeLinejoin="round"
        strokeWidth={2}
        d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4"
      />
    </svg>
  );
}
