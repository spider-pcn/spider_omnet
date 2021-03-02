#!/usr/bin/env Rscript
library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
df <- read.csv("smallWorld50NodesQueueSizeData")
plot_filename <- args[1]

#df$qSize <- as.factor(df$qSize)

label_list <- c("woQ" = "Diffindo(No Queue Term)",
                 "woW" = "Diffindo (No Window)",
                 "wQW" = "Diffindo")

box_plot <- ggplot(df, aes(
        x=Demand, group=Demand, y=qSize, fill=Scheme
    )) +
    geom_boxplot(outlier.shape=NA,color="black") +
    ylab("Queue Size") +
    scale_fill_manual(limit=c("woQ", "woW", "wQW"),
        values = c(
            "woQ" = "#d95f02",
            "woW" = "#1b9e77",
            "wQW" = "#e7298a"),
        labels=label_list
    ) +
    coord_cartesian(ylim=c(-1,50)) +
    theme_minimal() +
    theme(
        legend.position="top",
        legend.title=element_blank()

    )
    
ggsave(plot_filename, width=5.7, height=5)
