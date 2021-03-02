#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
args <- commandArgs(trailingOnly=TRUE)
file <- args[1] 
plot_filename <- args[2]
data<-read.csv(file)
data$Topo <- factor(data$Topo, levels=c("Small World", "Scale Free", "Lightning Topology"), ordered=T)
data$SchedulingAlg <- factor(data$SchedulingAlg, levels=c("SPF", "EDF", "FIFO", "LIFO"), ordered=T)
rel_schemes <- data[ which(data$Scheme=='DCTCP_qdelay'),]

label_list <- c("FIFO" = " First-In-First-Out    ",
                 "LIFO" = " Last-In-First-Out    ",
                 "EDF" = " Earliest-Deadline-First    ",
                 "SPF" = " Smallest-Payment-First    ")

color_list <- c(
                "FIFO" = "#c51b8a",
                "LIFO" = "#1b9e77",
                "SPF" = "#74a9cf",
                "EDF" = "#d95f0e")

pos <- "top"
height <- 3

# add a line plot
succ_ratio_plot <- ggplot(rel_schemes, aes_string(x="Topo", y="SuccRatio", fill="SchedulingAlg")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black") +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=.3, 
                      position=position_dodge(.9)) +
        labs(x="Topology", y="Success Ratio (%)") +
        scale_fill_manual(values=color_list,
                          labels=label_list)+
        theme_minimal(base_size=10) +
        theme(legend.title=element_blank())+
        guides(fill=guide_legend(ncol=3)) +
        theme(plot.margin=margin(c(0.5,0,0,0.5))) +
        theme(legend.text=element_text(size=rel(0.85)), 
              legend.key.size=unit(10,"points"),
              legend.position=pos, legend.margin=margin(c(0,5,1,5)))

ggsave(plot_filename, width=4, height=1.5)
