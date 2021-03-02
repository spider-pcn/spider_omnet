#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1] 
plot_filename <- args[2]
data<-read.csv(file)
data$NumPaths <- as.factor(data$NumPaths)
data$Topo <- factor(data$Topo, levels=c("Small World", "Scale Free", "Lightning Topology"), ordered=T)

#rel_schemes <- data[ which(data$Scheme=='DCTCP_qdelay'),]

label_list <- c(
                "1" = "  1  ",
                "2" = "  2  ",
                "4" = "  4  ",
                "8" = "  8  ")

color_list <- c(
                "1" = "#a1dab4",
                "2" = "#41b6c4",
                "4" = "#2c7fb8",
                "8" = "#253494")
pos <- "top"
height <- 3


# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="Topo", y="SuccRatio", fill="NumPaths")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black") +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=.3, 
                      position=position_dodge(.9)) +
        labs(x="Topology", y="Success Ratio (%)") +
        scale_fill_manual(values=color_list, labels=label_list) +
        theme_minimal(base_size=10) +
        theme(plot.margin=margin(c(0.5,0,0,0.5))) +
        theme(legend.title=element_blank()) +
        theme(legend.text=element_text(size=rel(0.85)), 
              legend.key.size=unit(15,"points"),
              legend.position=pos, legend.margin=margin(c(0,5,1,5)))


ggsave(plot_filename, width=4, height=1.5)
