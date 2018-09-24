package com.infomaximum;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.CopyOption;
import java.nio.file.Files;
import java.nio.file.Path;

public class ExeFileBuilder {

    public static class Info {

        private final String productName;
        private final String productVersion;
        private final Path fileIcon;
        private final String commandLine;
        private final String workingDir;
        private final boolean runAsAdmin;
        private final Path zipFile;

        public Info(String productName, String productVersion, Path fileIcon, String commandLine, String workingDir, boolean runAsAdmin, Path zipFile) {
            this.productName = productName;
            this.productVersion = productVersion;
            this.fileIcon = fileIcon;
            this.commandLine = commandLine;
            this.workingDir = workingDir;
            this.runAsAdmin = runAsAdmin;
            this.zipFile = zipFile;
        }

        public String getCommandLine() {
            return commandLine;
        }

        public String getWorkingDir() {
            return workingDir;
        }

        public boolean isRunAsAdmin() {
            return runAsAdmin;
        }

        public Path getZipFile() {
            return zipFile;
        }

        public String getProductVersion() {
            return productVersion;
        }

        public String getProductName() {
            return productName;
        }

        public Path getFileIcon() {
            return fileIcon;
        }
    }

    public void build(Info info, Path dstFilePath, CopyOption... options) throws IOException {
        try (InputStream zipDataStream = Files.newInputStream(info.zipFile)) {
            // do something
        }
    }
}
